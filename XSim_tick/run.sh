sst --add-lib-path build config.py -- --program test_12.m --latencies latencies.json --output stats.json

#$ added --output stats.json so that the output file is passed to the config.py and then to the core