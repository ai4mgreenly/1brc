# 1BRC Experiments

Inspired by the [One Billion Row Challenge](https://github.com/gunnarmorling/1brc), this project compares AI models' ability to generate high-performance code.

This repository is for learning purposes only - not intended for submission or sharing.

## Leaderboard

| Rank | Model | Thinking | Best Time | Notes |
|------|-------|----------|-----------|-------|
| 🥇 1 | Claude Opus 4.5 | Off | **2.854s** | 48 threads, warm cache |
| 🥈 2 | Claude Sonnet 4.5 | Off | 37.5s | Single-threaded |

## The Meta Prompt

Create a sub-agent using the model and thinking mode specified in the ./README.md file and let it complete the challenge without providing it any additional context. Take note of it's start and end times and provide an elapsed runtime when it's done.

# Model Instructions

- **Model**: [VARIES]
- **Thinking**: [VARIES]

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

## Prerequisites

- Java 21+ (for data generation scripts)

## Initial Setup

After cloning the repository, generate the test data (one-time setup):

```bash
# Build the 1BRC tools
cd original
./mvnw clean verify -DskipTests

# Generate 1 billion rows of test data (~14GB)
./create_measurements.sh 1000000000
mv measurements.txt ../data/

# Generate expected output for validation
./calculate_average_baseline.sh > ../data/expected_output.txt
```

## Data

The `data/` folder will contain (after setup):

- **`measurements.txt`** - 1 billion rows of test data (~14GB, gitignored)
- **`expected_output.txt`** - Correct output for validation (~250KB, gitignored)

## Benchmarking

To benchmark an implementation, redirect output to `/dev/null` to measure pure computation time:

```bash
time ./1brc ../data/measurements.txt > /dev/null
```

## Validation

To validate correctness, compare output against the expected result:

```bash
diff <(./1brc ../data/measurements.txt) ../data/expected_output.txt
```

No output means the implementation is correct.

## Structure

Each subdirectory represents a single experiment:

```
experiment-name/
├── README.md      # Prompt, model, thinking level, and benchmark results
└── ...            # Generated code
```

## Experiment README Format

Each experiment's README should include:

- **Model**: Which AI model generated the code
- **Thinking Level**: Thinking/reasoning mode used (if applicable)
- **Prompt**: The exact prompt given to the model
- **Benchmark Results**: Performance metrics from running against the 1brc dataset
