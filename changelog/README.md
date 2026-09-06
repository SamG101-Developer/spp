# Changelog

One file per released version, named `<version>.md`, matching the version in the repository-root `VERSION` file.

A pull request that changes compiler code must bump `VERSION` and add the matching entry here; the "Version &
Changelog" job on the PR enforces both. The file's contents become the body of the GitHub release that the
post-merge pipeline publishes.
