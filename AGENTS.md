# Assetto Corsa replay to telemetry

### Route Fast

- Tooling entrypoint: `mise.toml`
- Planning source: `.specify/memory/constitution.md`
- C++ / wasm rules for `cpp/`: `cpp/AGENTS.md`
- TS wasm wrapper: `packages/acreplay-wasm/`
- TS pack reader/validator: `packages/actelemetry/`

<!--VITE PLUS START-->

`vp` handles the full development lifecycle. Run `vp help` and `vp <command> --help` for information.

### Develop and buld commands

- `run`, `dlx`, `dev`, `check`, `lint`, `fmt`, `test`, `build`, `pack`, `preview`

### Execute commands

- exec - Execute a command from local `node_modules/.bin`
- cache - Manage the task cache

### Manage Dependencies

- add - Add packages to dependencies
- remove (`rm`, `un`, `uninstall`) - Remove packages from dependencies
- update (`up`) - Update packages to latest versions
- dedupe - Deduplicate dependencies
- outdated - Check for outdated packages
- list (`ls`) - List installed packages
- why (`explain`) - Show why a package is installed
- info (`view`, `show`) - View package information from the registry
- link (`ln`) / unlink - Manage local package links
- pm - Forward a command to the package manager

## Common Pitfalls

- Do not use pnpm, npm directly. `vp` can handle all package manager operations.
- **Always use Vite commands to run tools:** Don't attempt to run `vp vitest` or `vp oxlint`. They do not exist. Use `vp test` and `vp lint` instead.
- **Running scripts:** Vite+ commands take precedence over `package.json` scripts. If there is a `test` script defined in `scripts` that conflicts with the built-in `vp test` command, run it using `vp run test`.
- **Do not install Vitest, Oxlint, Oxfmt, or tsdown directly:** Vite+ wraps these tools. They must not be installed directly. You cannot upgrade these tools by installing their latest versions. Always use Vite+ commands.
- **Use Vite+ wrappers for one-off binaries:** Use `vp dlx` instead of package-manager-specific `dlx`/`npx` commands.
- **Import JavaScript modules from `vite-plus`:** Instead of importing from `vite` or `vitest`, all modules should be imported from the project's `vite-plus` dependency. For example, `import { defineConfig } from 'vite-plus';` or `import { expect, test, vi } from 'vite-plus/test';`. You must not install `vitest` to import test utilities.
- **Type-Aware Linting:** There is no need to install `oxlint-tsgolint`, `vp lint --type-aware` works out of the box.

<!--VITE PLUS END-->
