# AI Agents

Configure an AI agent with instructions, a model, and the tools it can use.
Run it in the Agents Explorer, through Bee, or in code. The runner uses
`chatWithTools` loops to send tool results back to the model.

## Requirements

- PHP 8.2 or later
- `ai` module
- `ai_tools` module

## Installation

- Install this module using the official [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).

## Configuration

1. Go to **Administration > Configuration > AI > AI Agents** (`admin/config/ai/ai-agents`).
2. Add an agent with a name, system prompt, model, and enabled tools.
   Choose a model that supports tool calling if the agent uses tools.
3. Use the Bee CLI or call `ai_agents_run()` to invoke an agent by machine name.

## Documentation

Find setup, examples, and troubleshooting in the [project wiki](https://github.com/backdrop-contrib/ai_agents/wiki).

## Issues

Bugs and feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/ai_agents/issues).

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).

- Developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
