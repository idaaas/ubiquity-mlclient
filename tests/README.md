# Integration tests

All tests assert logs of client and server. If logs are modified, corresponding tests are going to fail

## run all tests

```sh
  earthly -P +test-all --debug=true
```
arguments:
- debug: **optional** - logs from server and clien for each run.

## run one test

```sh
  earthly -P +run-scenario --scenario=scenario3 --debug=true
```
arguments:
- scenario: scenario to run.
- debug: **optional** - logs files from client and server.

## create one test

To create a new scenario, follow these steps:
- add a scenarioN folder where N=next index of scenario
- create **application.json** file and **Client.java** file in folder with your scenario
- add scenario description in Client.java file
- run your new scenario alone with **--no-cache** option
- add a new test file in test/java/integrationTests folder and write your assertions
- increment index in the "for loop"f of the "test-all" step

### mock syntax

coming soon