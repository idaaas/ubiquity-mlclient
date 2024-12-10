import gzip
import shutil

def unzip(gzip_file, output_file):

    # Open the gzip file and write the decompressed content to the output file
    with gzip.open(gzip_file, 'rb') as gz:
        with open(output_file, 'wb') as out_file:
            shutil.copyfileobj(gz, out_file)

    print(f"File extracted to {output_file}", flush=True)