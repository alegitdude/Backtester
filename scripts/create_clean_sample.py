import zstandard as zstd
import io

def create_clean_sample(input_path, output_path, line_limit):
    print(f"Sampling {line_limit} lines from {input_path}...")
    
    with open(input_path, 'rb') as fh_in, open(output_path, 'wb') as fh_out:
        dctx = zstd.ZstdDecompressor()
        cctx = zstd.ZstdCompressor()
        
        reader = dctx.stream_reader(fh_in)
        writer = cctx.stream_writer(fh_out)
        
        text_stream = io.TextIOWrapper(reader, encoding='utf-8')
        
        count = 0
        for line in text_stream:
            writer.write(line.encode('utf-8'))
            count += 1
            if count >= line_limit:
                break
                
        writer.close() 
    print("Done.")

create_clean_sample("../test/test_data/ES-glbx-20251105.mbp-10.csv.zst", "../test/test_data/ES-glbx-20251105.mbp-10-sample.csv.zst", 338821)