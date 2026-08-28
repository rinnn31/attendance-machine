import os
import gzip
import shutil

Import("env")

def prepare_and_compress_data():
    print("\n" + "="*45)
    print(">>> [AttendX] DANG NEN DATA SANG GZ...")
    print("="*45)

    original_data_dir = env.get("PROJECT_DATA_DIR")
    build_dir = env.subst("$BUILD_DIR")
    temp_data_dir = os.path.join(build_dir, "data_gz")

    if not original_data_dir or not os.path.exists(original_data_dir):
        print(f"Khong tim thay thu muc: {original_data_dir}")
        return

    if os.path.exists(temp_data_dir):
        shutil.rmtree(temp_data_dir)
    os.makedirs(temp_data_dir, exist_ok=True)

    for root, dirs, files in os.walk(original_data_dir):
        rel_path = os.path.relpath(root, original_data_dir)
        target_dir = os.path.join(temp_data_dir, rel_path) if rel_path != "." else temp_data_dir
        os.makedirs(target_dir, exist_ok=True)

        for file in files:
            src_file = os.path.join(root, file)
            if file.endswith(('.html', '.css', '.js', '.json', '.svg', '.txt', '.ico')):
                gz_file = os.path.join(target_dir, file + '.gz')
                with open(src_file, 'rb') as f_in:
                    with gzip.open(gz_file, 'wb', compresslevel=9) as f_out:
                        shutil.copyfileobj(f_in, f_out)
                print(f"Compressed: {file} -> {file}.gz")
            else:
                shutil.copy2(src_file, target_dir)

    env["PROJECT_DATA_DIR"] = temp_data_dir
    print(f">>> PROJECT_DATA_DIR moi: {temp_data_dir}")
    print("="*45 + "\n")

prepare_and_compress_data()