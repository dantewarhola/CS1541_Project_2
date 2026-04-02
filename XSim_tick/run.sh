sst --add-lib-path build config.py -- --program phase_4_2.m --configuration configuration.json --output stats.json

#$ added --output stats.json so that the output file is passed to the config.py and then to the core
#$ added --configuration configuration.json so that the configuration file is passed to the config.py and then to the core