#!/usr/bin/env python3
"""
Convert WAV file to C header with embedded data
"""

import sys
import struct


def parse_wav(filename):
    with open(filename, "rb") as f:
        # Read RIFF header
        riff = f.read(4)
        if riff != b"RIFF":
            print("Error: Not a RIFF file", file=sys.stderr)
            return None

        _file_size = struct.unpack("<I", f.read(4))[0]
        wave = f.read(4)
        if wave != b"WAVE":
            print("Error: Not a WAVE file", file=sys.stderr)
            return None

        # Find fmt chunk
        while True:
            chunk_id = f.read(4)
            if not chunk_id:
                break
            chunk_size = struct.unpack("<I", f.read(4))[0]

            if chunk_id == b"fmt ":
                fmt_data = f.read(chunk_size)
                audio_format = struct.unpack("<H", fmt_data[0:2])[0]
                num_channels = struct.unpack("<H", fmt_data[2:4])[0]
                sample_rate = struct.unpack("<I", fmt_data[4:8])[0]
                _byte_rate = struct.unpack("<I", fmt_data[8:12])[0]
                _block_align = struct.unpack("<H", fmt_data[12:14])[0]
                bits_per_sample = struct.unpack("<H", fmt_data[14:16])[0]

                print(f"// WAV Format: {audio_format} (1=PCM)", file=sys.stderr)
                print(f"// Channels: {num_channels}", file=sys.stderr)
                print(f"// Sample Rate: {sample_rate} Hz", file=sys.stderr)
                print(f"// Bit Depth: {bits_per_sample}", file=sys.stderr)

            elif chunk_id == b"data":
                data_size = chunk_size
                data = f.read(chunk_size)
                print(f"// Data Size: {data_size} bytes", file=sys.stderr)

                return {
                    "sample_rate": sample_rate,
                    "channels": num_channels,
                    "bits_per_sample": bits_per_sample,
                    "data": data,
                    "data_size": data_size,
                }
            else:
                # Skip unknown chunk
                f.seek(chunk_size, 1)

        return None


def generate_c_header(wav_info, output_file):
    with open(output_file, "w") as f:
        f.write("/* Auto-generated WAV data */\n\n")
        f.write("#ifndef CHIME_WAV_DATA_H\n")
        f.write("#define CHIME_WAV_DATA_H\n\n")
        f.write("#include <stdint.h>\n\n")

        f.write(f"#define CHIME_SAMPLE_RATE {wav_info['sample_rate']}\n")
        f.write(f"#define CHIME_CHANNELS {wav_info['channels']}\n")
        f.write(f"#define CHIME_BITS_PER_SAMPLE {wav_info['bits_per_sample']}\n")
        f.write(f"#define CHIME_DATA_SIZE {wav_info['data_size']}\n\n")

        f.write("const uint8_t chime_wav_data[] = {\n")

        data = wav_info["data"]
        for i in range(0, len(data), 16):
            chunk = data[i : i + 16]
            hex_values = ", ".join(f"0x{b:02x}" for b in chunk)
            f.write(f"    {hex_values},\n")

        f.write("};\n\n")
        f.write("#endif /* CHIME_WAV_DATA_H */\n")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.wav> <output.h>")
        sys.exit(1)

    wav_info = parse_wav(sys.argv[1])
    if wav_info:
        generate_c_header(wav_info, sys.argv[2])
        print(f"Generated {sys.argv[2]}", file=sys.stderr)
    else:
        print("Failed to parse WAV file", file=sys.stderr)
        sys.exit(1)
