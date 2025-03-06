#image_generator.py
import os
import random
import uuid
import time
from PIL import Image
import shutil

def generate_images_and_duplicates(output_folder, width, height, num_images, num_duplicates):
    """
    Generate 'num_images' unique PNG images of size width x height and then create
    'num_duplicates' duplicate images (copies of randomly chosen unique images)
    with new random names so that all files are mixed.
    """
    os.makedirs(output_folder, exist_ok=True)
    print(f"Generating {num_images} unique PNG images in '{output_folder}'...")
    unique_files = []
    start_time = time.time()
    
    for _ in range(num_images):
        # Generate random pixel data for an RGB image
        data = bytearray(random.getrandbits(8) for _ in range(width * height * 3))
        img = Image.frombytes("RGB", (width, height), bytes(data))
        filename = os.path.join(output_folder, f"{uuid.uuid4().hex}.png")
        img.save(filename, "PNG")
        unique_files.append(filename)
    
    print(f"Creating {num_duplicates} duplicate PNG images in '{output_folder}'...")
    for _ in range(num_duplicates):
        original_file = random.choice(unique_files)
        duplicate_filename = os.path.join(output_folder, f"{uuid.uuid4().hex}.png")
        shutil.copyfile(original_file, duplicate_filename)
    
    elapsed_time = time.time() - start_time
    print("Image generation and duplication complete.")
    print(f"Time taken: {elapsed_time:.2f} seconds")

if __name__ == "__main__":
    output_folder = "generated_images"
    width = 800
    height = 600
    num_images = 1000
    num_duplicates = 200
    generate_images_and_duplicates(output_folder, width, height, num_images, num_duplicates)