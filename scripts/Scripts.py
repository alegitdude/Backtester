import zstandard as zstd
import io

def create_clean_sample(input_path, output_path, line_limit):
    print(f"Sampling {line_limit} lines from {input_path}...")
    
    with open(input_path, 'rb') as fh_in, open(output_path, 'wb') as fh_out:
        dctx = zstd.ZstdDecompressor()
        cctx = zstd.ZstdCompressor()
        
        # Stream reader (Decompress)
        reader = dctx.stream_reader(fh_in)
        # Stream writer (Compress)
        writer = cctx.stream_writer(fh_out)
        
        # Use Text wrapper to handle line endings automatically
        text_stream = io.TextIOWrapper(reader, encoding='utf-8')
        
        count = 0
        for line in text_stream:
            # Write the clean, full line
            writer.write(line.encode('utf-8'))
            count += 1
            if count >= line_limit:
                break
                
        writer.close() # Safely closes the ZSTD frame
    print("Done.")

# Run this once to generate your test artifacts
create_clean_sample("/home/r/Desktop/11_05_25_ES_FUT/MBP-10/glbx-mdp3-20251105.mbp-10.csv.zst", "/home/r/Desktop/11_05_25_ES_FUT/ES-glbx-20251105.mbp-10-sample.csv.zst", 338821)