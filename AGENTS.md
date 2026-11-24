# Instructions for coding agents

Manifold is a hobby first-person fantasy game in C++20, built with CMake. See [README.md](README.md) for
building and running, [docs/ROADMAP.md](docs/ROADMAP.md) for plans and technical decisions,
[docs/DECISIONS.md](docs/DECISIONS.md) for important decisions with their reasons, and
[GAME_MANUAL.md](GAME_MANUAL.md) for controls.

## Conventions

Follow [docs/CODING_STYLE.md](docs/CODING_STYLE.md): formatting, naming, units and coordinates, random numbers, and
how code is documented with `///` comments.

- Make every random choice with the helpers of `src/util/random.hpp` (`random_index`, `random_int`, `random_float`,
  `random_chance`, `random_weighted_index`, `random_shuffle`, ...), never with the standard distributions,
  `std::shuffle` or `std::sample`, whose results differ between standard libraries, so a seed would give other levels
  on another platform. A test fails otherwise. See the Random numbers section of CODING_STYLE.md.

## Before finishing a change

```sh
cmake --build build --target format
cmake --build build                       # must build without warnings
ctest --test-dir build --output-on-failure
scripts/lint_changed.py                   # must pass
```

`scripts/lint_changed.py` runs the checks of the `lint` target only on the sources changed since the last
commit, and on the .cpp files that include a changed header. It lints everything if `.clang-tidy`,
`.clang-format`, `CMakeLists.txt`, `cmake/` or `external/` changed. To check a change that is already committed, pass
`--base` with the commit before its first commit (e.g. `--base HEAD~1` for one commit), or name the files or
folders to lint. The full `cmake --build build --target lint` takes a few minutes; use it only when
asked or when unsure what a change affects.

Each test fails after 60 s, so a test that hangs is named instead of stopping the run. In tests, a loop that waits
for something, such as the player landing, has a limit: use `run_until()` of `tests/game_test.cpp` or the like.

After a change to movement, collision, doors or level generation, also run the movement simulation, which must
print `OK` (it takes about a minute):

```sh
./build/manifold_sim   # options such as --layout debug --seeds 100 --seconds 60; a wrong option prints the usage
```

It plays 40 seeds of each layout with random walking, running, stepping aside, standing, turning, looking and jumping,
and opens and closes doors near the player, and checks the player's movement after every frame: no brief losses of
the ground, no overlaps with the geometry or closed doors, no endless falls, no speeds above running. It exits with 1
if one is broken, and lists examples with their layout, seed, level and frame (`--help` tells more). A run with more
seeds or longer is worth it for a change that touches collision itself.

- Python scripts in `scripts/` follow [docs/CODING_STYLE.md](docs/CODING_STYLE.md#python-scripts). After
  changing one, run the Python checks, which must pass:

  ```sh
  poetry -C scripts install    # once, to install the packages and tools
  cd scripts && poetry run black . && poetry run ruff check . && poetry run pyrefly check
  ```

- Credit every third-party asset (textures, sounds, models, fonts, ...), even when its license doesn't
  require it: give each pack a `README.md` in its directory under `assets/` (source link, author, license,
  which files were taken, how they were processed), and list it in the Credits section of
  [README.md](README.md). Only add assets whose license allows redistributing the files in this repo.
- Add or update unit tests in `tests/` for new logic. Code that needs a window or OpenGL is kept out of
  `manifold_core`, so that everything else can be tested.
- A bug fix comes with a test that fails without the fix. Check that it does before committing: stash the fix
  (`git stash push FILE...`), build and run the test, then `git stash pop`. For a fix that is already committed, put
  the old files back with `git checkout HEAD~1 -- FILE...`, build and run the test, then restore them with
  `git checkout HEAD -- FILE...`.
- For experiments, such as reproducing a bug, measuring something over many seeds or checking old code, write a
  throwaway program in a scratch directory outside the repo and build it against `manifold_core` with
  `scripts/scratch_build.py FILE.cpp [--run ARGS...]`. Don't commit such programs; turn what they check into a test
  or into the movement simulation if it should be kept.
- When a change alters a constant, a behavior or a name, search the docs (`docs/`, `README.md`, `GAME_MANUAL.md`,
  `AGENTS.md`) and the comments for its old value and name, e.g. `grep -rn "3 m/s"`, and update what no longer
  holds, also in older entries of `docs/DECISIONS.md` (with a note in their Status line).
- When changing key bindings or cheats, update [GAME_MANUAL.md](GAME_MANUAL.md).
- If a new system package is needed, add it to the install command in [README.md](README.md) and to
  `APT_PACKAGES` in [.github/workflows/ci.yml](.github/workflows/ci.yml).
- To check rendering or levels, run the game with console commands (see the Console section of
  [GAME_MANUAL.md](GAME_MANUAL.md)) and look at the screenshots, e.g.
  `./build/manifold SEED --exec "level_info; arrow on; cutaway HEIGHT; camera X Y 40 90 -90; screenshot FILE.png 800 450; quit"`.
  Small screenshots (800 x 450) are usually enough. For room levels, `level_info` tells how tiles map to
  world coordinates and the `cutaway` height of each layer, and `player_info` tells where the player is and how
  they move. `--screenshot FILE.png` saves the first frame. Runs with `--exec` always use a window and never
  save; to check a normal run, e.g. in full screen, pipe the commands into it instead, see the Console section of
  GAME_MANUAL.md.
  Add console commands rather than command-line options for new ways to inspect the game.
- When adding or changing console commands, update [GAME_MANUAL.md](GAME_MANUAL.md).

## Review after nontrivial changes

After a nontrivial change to code or docs (not typo fixes or small edits of comments or docs), and after the
checks above pass (a change to docs only needs none), commit the change and then start one subagent to review
it. The review of a change to docs looks for claims that don't match the code, other docs or git history, and
for unclear text:

- Give the subagent the commit (or diff) to review and the goal of the change.
- Tell it not to use any review skill (such as `/code-review`) and not to start subagents of its own.
- For a change to code, tell it that formatting, build, tests and lint already passed, so it shouldn't run
  them again; for a change to docs only, that there is nothing to build or run. It may build or run other
  small checks if a specific finding needs them.
- Ask it to report concrete findings (bugs, missed cases, docs that no longer match the code, departures
  from the conventions), each with a file and line, most severe first.

Then address the findings:

- Fix the clear ones, normally in a separate commit after the reviewed one, and run the checks again if
  code changed.
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
  previous commit" changes into the commits they belong to before pushing.
- Don't force-push the branch of an open PR unless the user explicitly asks: add new commits instead of
  rewriting pushed ones. A PR needn't be kept up to date with `main`, since GitHub rebases it when merging.
  If it conflicts with `main`, tell the user; rebase it onto `origin/main` and push it with
  `--force-with-lease` only when they ask. Never force-push `main`.
- End everything you post on GitHub (PR descriptions, reviews, each review comment, replies and other
  comments) with the line `🤖 Generated with [Claude Code](https://claude.com/claude-code)`.

When the user asks for a PR:

1. Before starting, ask the user about the decisions that aren't obvious from the request, the code and the
   docs (design trade-offs, scope). Then do the whole task without stopping.
2. Make the change on a new branch, with the checks of "Before finishing a change". If the change is
   substantial, affects gameplay and has plausible alternatives, it also adds a decision to
   [docs/DECISIONS.md](docs/DECISIONS.md), with the status **ACCEPTED**, in the format of the entries there:
   the decision, its gains, the rejected alternatives and the drawbacks.
3. Push the branch and open the PR with `gh pr create`. Its title is like a commit summary, and its
   description has these sections:
   - **Goal**: why the change is made.
   - **Solution**: a summary of the technical solution.
   - **Pros and cons** of the solution, as two separate lists under "Pros:" and "Cons:", and the alternatives that
     were considered, if any.
   - **Testing**: how the change was checked: tests added, screenshots looked at, and so on.
   - **TODO**, only if a screenshot would show the change (rendering, levels, UI): a request to the user to
     add one, with the console command that takes it and the screenshot's path, if one was taken. `gh`
     can't upload images, so the user adds it.
4. Review every PR, however small, also one that changes only docs, as in "Review after nontrivial
   changes", giving the subagent the PR's number and diff. Post the findings as one GitHub review with the
   event `COMMENT` (the author of a PR can't approve it or request changes), each finding as a comment on
   its line of the diff: `gh api repos/OWNER/REPO/pulls/NUMBER/reviews` with `comments` of
   `path`, `line` and `side` (`RIGHT` for added and unchanged lines, `LEFT` for removed ones). Findings about
   lines outside the diff go into the review's body. If there are no findings, the review says so.
5. Address the findings as for other changes, with new commits on the branch, then push them and reply to
   each review comment: fixed in which commit, or rejected and why. Match each reply to its comment by the
   comment's `id` from the response that posted the review, not by path and line, which can change after a push.
   Check that no reply is empty, and list the replies afterwards to verify.
6. Don't wait for CI: the local checks are faster. Tell the user the PR's link. The user merges it once CI
   is green (a PR that changes only `.md` files runs no CI); don't merge it yourself.
