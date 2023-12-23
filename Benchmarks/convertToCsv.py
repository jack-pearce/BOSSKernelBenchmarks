import pandas as pd
import json
import sys
from datetime import datetime
import shutil

if len(sys.argv) != 2:
    print("Usage: python script.py <file_name_to_append>")
    sys.exit(1)

with open('build/raw_results/result.json') as f:
    data = json.load(f)

first_benchmark_name = data['benchmarks'][0]['name'].replace('/', '_')
command_line_argument = sys.argv[1]
current_datetime = datetime.now().strftime("%Y%m%d_%H%M%S")

file_name = f'{first_benchmark_name}_{command_line_argument}_{current_datetime}'

df = pd.json_normalize(data['benchmarks'])
df.to_csv(f'results/csv/{file_name}.csv', index=False)

json_file_new_path = f'results/json/{file_name}.json'
shutil.copy('build/raw_results/result.json', json_file_new_path)