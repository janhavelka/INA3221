"""Configure upload-tool behavior after the PlatformIO builder is loaded."""

Import("env")  # type: ignore[name-defined]  # PlatformIO / SCons built-in

upload_flags = list(env.get("UPLOADERFLAGS", []))  # type: ignore[name-defined]
if "write-flash" in upload_flags and "--no-progress" not in upload_flags:
    write_index = upload_flags.index("write-flash")
    upload_flags.insert(write_index + 1, "--no-progress")
    env.Replace(UPLOADERFLAGS=upload_flags)  # type: ignore[name-defined]
