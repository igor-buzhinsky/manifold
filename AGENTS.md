# Instructions for coding agents

Manifold is a hobby first-person fantasy game in C++20, built with CMake. See [README.md](README.md) for
building and running, [docs/ROADMAP.md](docs/ROADMAP.md) for plans and technical decisions,
[docs/DECISIONS.md](docs/DECISIONS.md) for important decisions with their reasons, and
[GAME_MANUAL.md](GAME_MANUAL.md) for controls.

## Conventions

Follow [docs/CODING_STYLE.md](docs/CODING_STYLE.md): formatting, naming, units and coordinates, and how
code is documented with `///` comments.

## Before finishing a change

```sh
cmake --build build --target format
cmake --build build                       # must build without warnings
ctest --test-dir build --output-on-failure
scripts/lint_changed.py                   # must pass
```

`scripts/lint_changed.py` runs the checks of the `lint` target only on the sources changed since the last
commit, and on the .cpp files that include a changed header. It lints everything if `.clang-tidy`,
`.clang-format`, `CMakeLists.txt` or `external/` changed. To check a change that is already committed, pass
`--base` with the commit before its first commit (e.g. `--base HEAD~1` for one commit), or name the files or
folders to lint. The full `cmake --build build --target lint` takes a few minutes; use it only when
asked or when unsure what a change affects.

- Python scripts in `scripts/` follow [docs/CODING_STYLE.md](docs/CODING_STYLE.md#python-scripts). After
  changing one, run the Python checks, which must pass:

  ```sh
  poetry -C scripts install    # once, to install the tools
  cd scripts && poetry run black . && poetry run ruff check . && poetry run pyrefly check
  ```

- Credit every third-party asset (textures, sounds, models, fonts, ...), even when its license doesn't
  require it: give each pack a `README.md` in its directory under `assets/` (source link, author, license,
  which files were taken, how they were processed), and list it in the Credits section of
  [README.md](README.md). Only add assets whose license allows redistributing the files in this repo.
- Add or update unit tests in `tests/` for new logic. Code that needs a window or OpenGL is kept out of
  `manifold_core`, so that everything else can be tested.
- When changing key bindings or cheats, update [GAME_MANUAL.md](GAME_MANUAL.md).
- If a new system package is needed, add it to the install command in [README.md](README.md) and to
  `APT_PACKAGES` in [.github/workflows/ci.yml](.github/workflows/ci.yml).
- To check rendering or levels, run the game with console commands (see the Console section of
  [GAME_MANUAL.md](GAME_MANUAL.md)) and look at the screenshots, e.g.
  `./build/manifold SEED --exec "level_info; arrow on; cutaway HEIGHT; camera X Y 40 90 -90; screenshot FILE.png 800 450; quit"`.
  Small screenshots (800 x 450) are usually enough. For room levels, `level_info` tells how tiles map to
  world coordinates and the `cutaway` height of each layer. `--screenshot FILE.png` saves the first frame.
  Add console commands rather than command-line options for new ways to inspect the game.
- When adding or changing console commands, update [GAME_MANUAL.md](GAME_MANUAL.md).

## Review after nontrivial changes

After a nontrivial code change (not typos, comments or docs only), and after the checks above pass, commit
the change and then start one subagent to review it:

- Give the subagent the commit (or diff) to review and the goal of the change.
- Tell it not to use any review skill (such as `/code-review`) and not to start subagents of its own.
- Tell it that formatting, build, tests and lint already passed, so it shouldn't run them again. It may
  build or run other small checks if a specific finding needs them.
- Ask it to report concrete findings (bugs, missed cases, docs that no longer match the code, departures
  from the conventions), each with a file and line, most severe first.

Then address the findings:

- Fix the clear ones, normally in a separate commit after the reviewed one, and run the checks again.
- If a finding needs a decision that isn't obvious (a design trade-off, a change of scope, a disagreement
  with the reviewer), ask the user before acting on it.
- Tell the user which findings were fixed, which were rejected and why.

## Git and pull requests

- Don't push to `main` unless the user explicitly asks for it. Committing to a local `main` is fine.
- A PR's work goes on its own branch, named `TYPE/SHORT-DESCRIPTION` in lowercase kebab case, where TYPE is
  `feature`, `fix`, `refactor`, `docs`, `ci` or `chore`: e.g. `feature/asset-lookup`, `fix/door-collision`.
  Start it from an up-to-date `origin/main`.
- PRs are merged by rebasing, so each of their commits lands on `main` as it is. Each commit must build
  without warnings and pass the tests and lint, and have a full commit message. Fold "WIP" or "fix the
  previous commit" changes into the commits they belong to before pushing. Once a review is posted, add new
  commits rather than rewriting the ones it refers to.
- To bring a PR branch up to date with `main`, rebase it onto `origin/main` and push it with
  `--force-with-lease`. Never force-push `main`.

When the user asks for a PR:

1. Before starting, ask the user about the decisions that aren't obvious from the request, the code and the
   docs (design trade-offs, scope). Then do the whole task without stopping.
2. Make the change on a new branch, with the checks of "Before finishing a change".
3. Push the branch and open the PR with `gh pr create`. Its title is like a commit summary, and its
   description has these sections:
   - **Goal**: why the change is made.
   - **Solution**: a summary of the technical solution.
   - **Pros and cons** of the solution, and the alternatives that were considered, if any.
   - **Testing**: how the change was checked: tests added, screenshots looked at, and so on.
   - **TODO**, only if a screenshot would show the change (rendering, levels, UI): a request to the user to
     add one, with the console command that takes it and the screenshot's path, if one was taken. `gh`
     can't upload images, so the user adds it.
4. Review any change to code, however small (only changes to docs and comments are left out), as in "Review
   after nontrivial changes", giving the subagent the PR's number and diff. Post the findings as one GitHub
   review with the event `COMMENT` (the author of a PR can't approve it or request changes), each finding as
   a comment on its line of the diff: `gh api repos/OWNER/REPO/pulls/NUMBER/reviews` with `comments` of
   `path`, `line` and `side` (`RIGHT` for added and unchanged lines, `LEFT` for removed ones). Findings about
   lines outside the diff go into the review's body. If there are no findings, the review says so.
5. Address the findings as for other changes, with new commits on the branch, then push them and reply to
   each review comment: fixed in which commit, or rejected and why.
6. Don't wait for CI: the local checks are faster. Tell the user the PR's link. The user merges it once CI
   is green; don't merge it yourself.
