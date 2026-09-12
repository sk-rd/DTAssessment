#!/usr/bin/env python3
"""Command-line entry point for DTAssessment JSON damage tolerance assessment."""

import argparse
from json_io import run


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="simulation input JSON")
    parser.add_argument("material", help="material properties JSON")
    parser.add_argument("output", help="assessment output JSON")
    args = parser.parse_args()
    run(args.input, args.material, args.output)


if __name__ == "__main__":
    main()
