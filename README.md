# Gateway SDK

The Gateway SDK is a software development kit designed to help developers configure their gateway boards to work with Wiliot. By programming and configuring the gateway boards using C language, developers can establish communication between the gateways and the bridges that energize Wiliot pixels and receive events/data from them. The configured gateways connect to the Wiliot cloud and transmit these events for further processing.

## Table of Contents

- [Getting Started](#getting-started)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Usage](#usage)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgments](#acknowledgments)

## Getting Started

These instructions will guide you on how to set up and use the project. 

TODO : [Provide a general overview of the steps required to get the project up and running.]

### Prerequisites

Software: You're expected to have any [Zephyr](https://docs.zephyrproject.org/latest/introduction/index.html)-based development environment. 
Our choice is [nRF Connect extension for VSCode](https://nrfconnect.github.io/vscode-nrf-connect/get_started/install.html). The extension allows you to edit, build & flash the project with an easy intuitive GUI once set up.

Hardware: You'll need your board, as well as any necessary hardware for flashing it.
In our goto case it's the [MG100](https://www.lairdconnect.com/iot-devices/bluetooth-iot-devices/sentrius-mg100-gateway-lte-mnb-iot-and-bluetooth-5) board and a J-Link([programming guide](https://www.lairdconnect.com/documentation/user-guide-programming-mg100)).
 
### Installation

1. Clone the repository:

```bash
git clone https://github.com/OpenAmbientIoT/GatewaySDK.git
```
2. Navigate to the repository directory:

3. Follow the instructions in the provided documentation to configure your development environment.


Provide step-by-step instructions on how to install and configure the project. Include any commands or scripts that may be needed to build and run the project.



## Usage

To use the Gateway SDK:

1. Open the example configuration file in your preferred C development environment.
2. Modify the example configuration file according to the specifications of your gateway board and the Wiliot bridge.
3. Compile and upload the modified configuration file to your gateway board.
4. Verify that your gateway board is successfully communicating with the Wiliot bridge and the Wiliot cloud.

Provide examples of how to use the project or library. Include code snippets, screenshots, or other visual aids to help users understand how to use the project effectively.

## Documentation

For detailed documentation on the Gateway SDK, API reference, and examples, please visit [link to the documentation].

If applicable, provide a link to the project's documentation, API reference, or any other resources that can help users better understand the project.

## Contributing

We welcome contributions to the Gateway SDK. Please follow the contributing guidelines outlined in [CONTRIBUTING.md](CONTRIBUTING.md) for reporting issues, submitting pull requests, or participating in the project's community.

## License

This project is licensed under the [LICENSE NAME] License - see the [LICENSE](LICENSE) file for details.
Include information about the project's license, as well as a link to the full license text.

## Acknowledgments

- Wiliot for their innovative IoT solutions and support.
- Any individuals or organizations that have contributed to the project.

Mention any individuals or organizations that have contributed to the project. You can also thank external libraries, tools, or resources that have been used in the project.
