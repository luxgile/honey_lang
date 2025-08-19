set -e

bundle_target() {
  cross build --release --target $1

  executable_name="honeyc"
  release_exe_path="target/${1}/release/${executable_name}"

  if [ ! -f "$release_exe_path" ]; then 
    release_exe_path="${release_exe_path}.exe"
    if [ ! -f "$release_exe_path" ]; then 
      echo "error: no honeyc executable found at ${release_exe_path}"
      exit 1
    fi
  fi

  bundle="bundle/${1}"
  if [ ! -d "$bundle" ]; then
    mkdir -p "$bundle"
  fi

  cp "$release_exe_path" "$bundle/"
  cp -r "std" "$bundle/"
  cp -r "honey" "$bundle/"

  # Cleaning the bundle dir
  rm "$bundle/std/meson.build"
  find "$bundle/std" -type f -name "*.c" -delete
  find "$bundle/std/build" -type f ! -name "libhoney_std.a" -delete
  find "$bundle/std/build" -type d -empty -delete
}

bundle_target x86_64-unknown-linux-gnu
bundle_target x86_64-pc-windows-gnu

echo "bundling complete!"
