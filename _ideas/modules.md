# Modules

- All module namespaces match their file paths. So a module located at `std/io/file.spp` will have the namespace
  `std::io::file`. The final directory is `io`, and the file is `file.spp`.
- But there is no way to have types or functions in the `std::io` namespace, because there is no file at
  `std/io.spp`, it is a folder.
- There is a solution, like Rust has `mod.rs`, where we have a specially named module in a folder, maybe `_.spp` or
  something unique, that will be the module for that folder. So `std/io/_.spp` will be the module for `std::io`, and
  can contain types and functions.
- Another thing is that we should probably prevent filenames and folders in the same directory from having the same
  name, to prevent confusion. So you can't have `std/io.spp` and `std/io/` existing at the same time.