#!/usr/bin/env python3
#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic


import argparse
import yaml
from os import path


def get_header_guard_start(filename):
    macro_name = filename.split('.h')[0]
    return '''/* File generated by %s, do not modify */
#ifndef %s_H__
#define %s_H__''' % (__file__, macro_name.upper(), macro_name.upper())


def get_header_guard_end(filename):
    return "#endif /* %s_H__ */" % filename.split('.h')[0].upper()


DEST_HEADER = 1
DEST_KCONFIG = 2


def get_config_lines(gpm_config, greg_config, head, split, dest, current_domain=None):
    config_lines = list()

    def string_of_strings(mlist):
        return '"%s"' % " ".join(["%s" % elem for elem in mlist])

    partition_id = 0

    for domain, pm_config in gpm_config.items():
        reg_config = greg_config[domain]
        def partition_has_device(p):
            return 'region' in p and 'device' in reg_config[p['region']] \
                   and reg_config[p['region']]['device']

        def add_line(text_before_split, text_after_split):
            if current_domain is None or domain == current_domain or "LABEL" in text_before_split:
                # Don't prefix with domain for the current domain
                config_lines.append("{}PM_{}{}{}".format(head, text_before_split, split, text_after_split))
            else:
                config_lines.append("{}PM{}_{}{}{}".format(
                    head, "_{}".format(domain) if domain is not None else "",
                    text_before_split, split, text_after_split))

        for name, partition in sorted(pm_config.items(),
                                      key=lambda key_value_tuple: key_value_tuple[1]['address']):

            add_line("%s_ADDRESS" % name.upper(), "0x%x" % partition['address'])
            add_line("%s_SIZE" % name.upper(), "0x%x" % partition['size'])
            add_line("%s_NAME" % name.upper(), "%s" % name)

            if partition_has_device(partition):
                add_line("%s_ID" % name.upper(), "%d" % partition_id)
                if current_domain is None or domain == current_domain:
                    add_line("%d_LABEL" % partition_id, "%s" % name.upper())
                else:
                    add_line("%d_LABEL" % partition_id, "%s_%s" % (domain, name.upper()))
                partition_id += 1

            if dest is DEST_HEADER:
                if partition_has_device(partition):
                    add_line("%s_DEV_NAME" % name.upper(), f"\"{reg_config[partition['region']]['device']}\"")
            elif dest is DEST_KCONFIG:
                if 'span' in partition.keys():
                    add_line("%s_SPAN" % name.upper(), string_of_strings(partition['span']))

        add_line("NUM", "%d" % partition_id)

        def find_depth(key, depth=0):
            if 'span' in pm_config[key].keys():
                return find_depth(pm_config[key]['span'][0], depth + 1)
            return depth

        flash_partition_pm_config = {k: v for k, v in pm_config.items()}
        all_by_size = list(flash_partition_pm_config.keys())
        all_by_size = sorted(all_by_size, key=find_depth)
        all_by_size = sorted(all_by_size, key=lambda key: flash_partition_pm_config[key]['size'])
        add_line("ALL_BY_SIZE", string_of_strings(all_by_size))

    return config_lines


def write_config_lines_to_file(pm_config_file_path, config_lines):
    with open(pm_config_file_path, 'w') as out_file:
        out_file.write('\n'.join(config_lines))


def write_gpm_config(gpm_config, regions_config, name, out_path):
    pm_config_file = path.basename(out_path)

    domain, image = name.split(':')
    config_lines = get_config_lines(gpm_config, regions_config, "#define ", " ", DEST_HEADER, domain)
    image_config_lines = list.copy(config_lines)

    image_config_lines.append("#define PM_ADDRESS 0x{:x}".format(gpm_config[domain][image]['address']))
    image_config_lines.append("#define PM_SIZE 0x{:x}".format(gpm_config[domain][image]['size']))

    image_config_lines.insert(0, "#include <autoconf.h>")
    image_config_lines.insert(0, get_header_guard_start(pm_config_file))

    image_config_lines.append(get_header_guard_end(pm_config_file))
    write_config_lines_to_file(out_path, image_config_lines)


def write_kconfig_file(gpm_config, regions_config, out_path):
    config_lines = get_config_lines(gpm_config, regions_config, "", "=", DEST_KCONFIG)
    write_config_lines_to_file(out_path, config_lines)


def parse_args():
    parser = argparse.ArgumentParser(
        description='''Creates files based on Partition Manager results.''',
        formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument("--input-partitions", required=True, type=str, nargs="+",
                        help="Paths to the input .yml files, one per domain.")

    parser.add_argument("--input-regions", required=True, type=str, nargs="+",
                        help="Paths to the input .yml files with region configurations, once per domain.")

    parser.add_argument("--config-file", required=False, type=str,
                        help="Path to the output .config file.")

    parser.add_argument("--images", required=False, type=str, nargs="+",
                        help="List of domain prefixed image partitions.")

    parser.add_argument("--header-files", required=False, type=str, nargs='+',
                        help="Paths to the output header files files."
                             "These will be matched to the --images.")

    return parser.parse_args()


def main():
    args = parse_args()

    gpm_config = dict()  # GLOBAL pm_config
    greg_config = dict()  # GLOBAL pm_regions

    for partition in args.input_partitions:
        fn = path.basename(partition)
        domain_name = fn[fn.index("partitions_") + len("partitions_"):fn.index(".yml")]
        with open(partition, 'r') as f:
            gpm_config[domain_name] = yaml.safe_load(f)

    for region in args.input_regions:
        fn = path.basename(region)
        domain_name = fn[fn.index("regions_") + len("regions_"):fn.index(".yml")]
        with open(region, 'r') as f:
            greg_config[domain_name] = yaml.safe_load(f)

    if args.config_file:
        write_kconfig_file(gpm_config, greg_config, args.config_file)

    if args.header_files:
        for name, header_file in zip(args.images, args.header_files):
            write_gpm_config(gpm_config, greg_config, name, header_file)


if __name__ == "__main__":
    main()
