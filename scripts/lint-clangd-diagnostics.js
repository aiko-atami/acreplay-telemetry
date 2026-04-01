import { spawn } from 'node:child_process';
import { readFile, readdir } from 'node:fs/promises';
import { dirname, extname, join, relative, resolve } from 'node:path';
import process from 'node:process';
import { fileURLToPath, pathToFileURL } from 'node:url';

const scriptDir = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(scriptDir, '..');
const cppRoot = resolve(repoRoot, 'cpp');
const compileCommandsPath = resolve(cppRoot, 'compile_commands.json');
const kTrackedExtensions = new Set(['.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx']);
const kQuietPeriodMs = 200;
const kDiagnosticsTimeoutMs = 30_000;

function logError(message) {
  process.stderr.write(`${message}\n`);
}

async function listCppFiles(dir) {
  const entries = await readdir(dir, { withFileTypes: true });
  const files = [];

  for (const entry of entries) {
    if (entry.name === 'build' || entry.name === '.git') {
      continue;
    }

    const fullPath = join(dir, entry.name);
    if (entry.isDirectory()) {
      files.push(...(await listCppFiles(fullPath)));
      continue;
    }

    if (kTrackedExtensions.has(extname(entry.name))) {
      files.push(fullPath);
    }
  }

  return files.sort();
}

function formatDiagnostic(filePath, diagnostic) {
  const line = (diagnostic.range?.start?.line ?? 0) + 1;
  const column = (diagnostic.range?.start?.character ?? 0) + 1;
  const severity = diagnostic.severity === 1 ? 'error' : 'warning';
  const code = diagnostic.code === undefined ? '' : ` ${String(diagnostic.code)}`;
  const source = diagnostic.source ? ` [${diagnostic.source}${code}]` : code ? ` [${code.trim()}]` : '';

  return `${relative(repoRoot, filePath)}:${line}:${column}: ${severity}${source} ${diagnostic.message}`;
}

class ClangdClient {
  constructor() {
    this.nextId = 1;
    this.pendingRequests = new Map();
    this.pendingDiagnostics = new Map();
    this.stdoutBuffer = Buffer.alloc(0);
    this.stderrBuffer = '';

    this.process = spawn(
      'clangd',
      [
        '--enable-config',
        '--compile-commands-dir=cpp',
        '--background-index=false',
        '--log=error',
      ],
      {
        cwd: repoRoot,
        stdio: ['pipe', 'pipe', 'pipe'],
      },
    );

    this.process.stdout.on('data', (chunk) => {
      this.stdoutBuffer = Buffer.concat([this.stdoutBuffer, chunk]);
      this.drainMessages();
    });

    this.process.stderr.on('data', (chunk) => {
      this.stderrBuffer += chunk.toString('utf8');
    });

    this.process.on('exit', (code, signal) => {
      const reason = signal ? `signal ${signal}` : `exit code ${code ?? 'unknown'}`;
      for (const { reject } of this.pendingRequests.values()) {
        reject(new Error(`clangd exited unexpectedly with ${reason}`));
      }
      this.pendingRequests.clear();

      for (const state of this.pendingDiagnostics.values()) {
        clearTimeout(state.timeoutId);
        clearTimeout(state.quietTimerId);
        state.reject(new Error(`clangd exited unexpectedly with ${reason}`));
      }
      this.pendingDiagnostics.clear();
    });
  }

  drainMessages() {
    while (true) {
      const headerEnd = this.stdoutBuffer.indexOf('\r\n\r\n');
      if (headerEnd === -1) {
        return;
      }

      const headerText = this.stdoutBuffer.subarray(0, headerEnd).toString('utf8');
      const lengthMatch = /^Content-Length: (\d+)$/im.exec(headerText);
      if (lengthMatch === null) {
        throw new Error(`clangd sent a response without Content-Length:\n${headerText}`);
      }

      const contentLength = Number(lengthMatch[1]);
      const messageEnd = headerEnd + 4 + contentLength;
      if (this.stdoutBuffer.length < messageEnd) {
        return;
      }

      const body = this.stdoutBuffer.subarray(headerEnd + 4, messageEnd).toString('utf8');
      this.stdoutBuffer = this.stdoutBuffer.subarray(messageEnd);
      this.handleMessage(JSON.parse(body));
    }
  }

  handleMessage(message) {
    if (message.id !== undefined) {
      const pending = this.pendingRequests.get(message.id);
      if (!pending) {
        return;
      }

      this.pendingRequests.delete(message.id);
      if (message.error) {
        pending.reject(new Error(`${message.error.message} (${message.error.code})`));
        return;
      }

      pending.resolve(message.result);
      return;
    }

    if (message.method !== 'textDocument/publishDiagnostics') {
      return;
    }

    const uri = message.params?.uri;
    if (typeof uri !== 'string') {
      return;
    }

    const state = this.pendingDiagnostics.get(uri);
    if (!state) {
      return;
    }

    state.latestDiagnostics = message.params?.diagnostics ?? [];
    clearTimeout(state.quietTimerId);
    state.quietTimerId = setTimeout(() => {
      clearTimeout(state.timeoutId);
      this.pendingDiagnostics.delete(uri);
      state.resolve(state.latestDiagnostics);
    }, kQuietPeriodMs);
  }

  send(message) {
    const payload = JSON.stringify(message);
    this.process.stdin.write(`Content-Length: ${Buffer.byteLength(payload, 'utf8')}\r\n\r\n${payload}`);
  }

  request(method, params) {
    const id = this.nextId++;
    const promise = new Promise((resolve, reject) => {
      this.pendingRequests.set(id, { resolve, reject });
    });
    this.send({ jsonrpc: '2.0', id, method, params });
    return promise;
  }

  notify(method, params) {
    this.send({ jsonrpc: '2.0', method, params });
  }

  async initialize() {
    await this.request('initialize', {
      processId: process.pid,
      rootUri: pathToFileURL(repoRoot).href,
      capabilities: {
        textDocument: {
          publishDiagnostics: {
            relatedInformation: true,
            versionSupport: false,
          },
        },
      },
      clientInfo: {
        name: 'acreplay-clangd-lint',
      },
      workspaceFolders: [
        {
          uri: pathToFileURL(repoRoot).href,
          name: 'acreplay-parser',
        },
      ],
    });
    this.notify('initialized', {});
  }

  async collectDiagnostics(filePath) {
    const uri = pathToFileURL(filePath).href;
    const text = await readFile(filePath, 'utf8');

    const diagnosticsPromise = new Promise((resolvePromise, rejectPromise) => {
      const timeoutId = setTimeout(() => {
        this.pendingDiagnostics.delete(uri);
        rejectPromise(new Error(`Timed out waiting for clangd diagnostics for ${relative(repoRoot, filePath)}`));
      }, kDiagnosticsTimeoutMs);

      this.pendingDiagnostics.set(uri, {
        latestDiagnostics: [],
        quietTimerId: undefined,
        timeoutId,
        resolve: resolvePromise,
        reject: rejectPromise,
      });
    });

    this.notify('textDocument/didOpen', {
      textDocument: {
        uri,
        languageId: 'cpp',
        version: 1,
        text,
      },
    });

    try {
      return await diagnosticsPromise;
    } finally {
      this.notify('textDocument/didClose', {
        textDocument: { uri },
      });
    }
  }

  async shutdown() {
    try {
      await this.request('shutdown', null);
    } finally {
      this.notify('exit');
    }
  }
}

try {
  await readFile(compileCommandsPath, 'utf8');
} catch {
  logError('cpp/compile_commands.json is missing. Run `mise run configure:cpp` first.');
  process.exit(2);
}

const client = new ClangdClient();
const files = await listCppFiles(cppRoot);
let issues = 0;

try {
  await client.initialize();

  for (const filePath of files) {
    const diagnostics = await client.collectDiagnostics(filePath);
    const relevantDiagnostics = diagnostics.filter((diagnostic) => {
      const severity = diagnostic.severity ?? 2;
      return severity <= 2 && diagnostic.source !== 'clang-tidy';
    });

    for (const diagnostic of relevantDiagnostics) {
      process.stdout.write(`${formatDiagnostic(filePath, diagnostic)}\n`);
      issues += 1;
    }
  }
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  if (client.stderrBuffer.trim()) {
    logError(client.stderrBuffer.trimEnd());
  }
  logError(message);
  process.exit(2);
} finally {
  await client.shutdown().catch(() => {});
}

if (issues > 0) {
  logError(`clangd reported ${issues} warning(s)/error(s).`);
  process.exit(1);
}

process.stdout.write(`clangd reported no IDE diagnostics across ${files.length} C/C++ files.\n`);
