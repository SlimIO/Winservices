# Winservices
Windows Services - Node.JS low level binding

## How to build the project

Before building the project, be sure to get the following npm package installed:

- Install (or upgrade to) NodeJS v10+ and npm v6+
- [Windows build tools](https://www.npmjs.com/package/windows-build-tools)

Then, just run normal npm install command:

```bash
$ npm install
```

## Available commands

All projects commands are described here:

| command | description |
| --- | --- |
| npm run prebuild | Generate addon prebuild |
| npm run doc | Generate JSDoc .HTML documentation (in the /docs root directory) |
| npm run coverage | Generate coverage of tests |
| npm run report | Generate .HTML report of tests coverage |

> the report command have to be triggered after the coverage command.
