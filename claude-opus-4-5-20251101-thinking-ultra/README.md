# Instructions

- **Model**: claude-opus-4-5-20251101
- **Thinking**: ultra

Your goal is to create the fastest possible non-portable "C" implementation using only gcc and standard libraries that will satisfy the following challenge.  When you've completed the challenge update the "## Results" section below with your benchmark timings and a brief description of your strategy.

## The Challenge

Read a file containing one billion temperature measurements and calculate min, mean, and max temperature per weather station.

**Input format:** `<station_name>;<temperature>`

```
Hamburg;12.0
Istanbul;6.2
Istanbul;23.0
```

- Station names: UTF-8 strings (1-100 bytes), excluding `;` and `\n`
- Temperature values: -99.9 to 99.9, always one decimal place
- Maximum 10,000 unique station names

**Output format (stdout):** Sorted alphabetically by station name, rounded to one decimal place:

```
{StationA=-23.0/18.0/59.2, StationB=-16.2/26.0/67.3, ...}
```

## Details

- Data file: `../data/measurements.txt`
- Expected output: `../data/expected_output.txt`

## Benchmarking

```bash
time ./1brc ../data/measurements.txt > /dev/null
```

## Validation

```bash
diff <(./1brc ../data/measurements.txt) ../data/expected_output.txt
```

## Results

*Not yet run*
