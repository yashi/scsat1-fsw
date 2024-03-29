import argparse
import os
import sys
import re
import yaml
from datetime import timezone, timedelta
from matplotlib import pyplot
from matplotlib.dates import DateFormatter
from matplotlib.backends.backend_pdf import PdfPages
from yamcs.client import YamcsClient

from datetime import datetime

DEFAULT_YAMCS_URL='localhost:8090'
YAMCS_INSTANCE='scsat1'

x_data = {}
y_data = {}
err_cnt = {}
configs = {}


def write_pdf():

    global x_data
    global y_data

    with open(configs['pdf-name'], "wb") as file, PdfPages(file) as pdf:
        for param in configs['parameters']:
            target = param['name']
            pyplot.title(f"{target} (err: {err_cnt[target]})")
            pyplot.plot(x_data[target], y_data[target])
            pyplot.gca().xaxis.set_major_formatter(DateFormatter("%H:%M", tz='Asia/Tokyo'))
            pyplot.gcf().autofmt_xdate()

            if 'TEMP' not in target and 'RW_' not in target:
                y_min, y_max = pyplot.gca().get_ylim()
                pyplot.gca().set_ylim(0, y_max * 1.1)

            pdf.savefig()
            pyplot.close()


def write_csv():

    global x_data
    global y_data

    for param in configs['parameters']:
        target = param['name']
        csvdir = configs['csv-dir']
        csvname = target.split('/')[-1]
        csvname = csvname.lower()
        with open(f"{csvdir}/{csvname}.csv", 'w',  encoding="utf-8") as outf:
            for idx, x in enumerate(x_data[target]):
                outf.write(f"{x},{y_data[target][idx]}\n")


def read_yamcs_archive(search_min):

    client = YamcsClient(configs['yamcs-url'])
    archive = client.get_archive(instance=YAMCS_INSTANCE)

    now = datetime.now(tz=timezone.utc)
    if search_min < 0:
        start = None
    else:
        start = now - timedelta(minutes=search_min)

    for param in configs['parameters']:
        target = param['name']
        stream = archive.stream_parameter_values(target, start=start, stop=now);
        for pdata in stream:
            for data in pdata:
                x_data[target].append(data.generation_time)
                y_data[target].append(data.eng_value)

        if not 'status' in param or param['status'] == "":
            continue

        stream = archive.stream_parameter_values(param['status']);
        for pdata in stream:
            for data in pdata:
                if data.eng_value != 0:
                    err_cnt[target] += 1


def init(yaml_file):

    global x_data
    global y_data
    global configs

    with open(yaml_file, 'r') as file:
        configs = yaml.safe_load(file)

    if not os.path.exists(configs['csv-dir']):
        os.makedirs(configs['csv-dir'])

    for param in configs['parameters']:
        target = param['name']
        x_data[target] = []
        y_data[target] = []
        err_cnt[target] = 0


def main(args):

    for yaml_file in args.yaml:

        if not os.path.exists(yaml_file):
            print(f"{yaml_file} is not exist, so skip the parse")
            continue
        else:
            print(f"Start to parse according to {yaml_file}")

        init(yaml_file)
        read_yamcs_archive(args.min)
        write_csv()
        write_pdf()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="SC-Sat1 Yamcs telemetry parse tool")
    parser.add_argument("--yaml", type=str, required=True, nargs="+",
        help="Target yaml files")
    parser.add_argument("--min", type=int, default=-1,
        help="Target minutes from current time for searching")
    args = parser.parse_args()
    main(args)
