## CircleCI build scripts

The `.circleci` directory contains scripts for each build step in each build stage.
These scripts have beep ported over from travis and define two stages `lint` and `test`. 
Every script in here is named and numbered according to which stage and lifecycle
step it belongs to.