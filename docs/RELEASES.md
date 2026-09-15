# Release process

1. Update `VERSION`, `CHANGELOG.md`, and README release references.
2. Run `get_idf`, then `idf.py set-target esp32s3` and `idf.py build`.
3. Test the exact build on physical hardware.
4. Create a merged release image and checksum:

```bash
cd build
esptool.py --chip esp32s3 merge_bin -o ../release/netguard-v0.1.0-esp32s3-16mb.bin @flash_args
cd ..
sha256sum release/netguard-v0.1.0-esp32s3-16mb.bin > release/netguard-v0.1.0-esp32s3-16mb.bin.sha256
```

5. Commit source, documents, binary, and checksum together.
6. Create a GitHub release tagged `v0.1.0`, then attach the binary and checksum.

Never publish an untested binary or a binary that does not match the tagged source.
