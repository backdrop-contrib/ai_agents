# AI Agents

Config-driven agent runner for the Backdrop CMS AI module. Enables defining agents with system prompts and tool access, then running them in chatWithTools loops via code or Bee CLI.

## Requirements

- `ai` module
- `ai_tools` module

## Installation

- Install this module using the official [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).

## Configuration

1. Go to **Administration > Configuration > AI > AI Agents** (`admin/config/ai/ai-agents`).
2. Add an agent configuration with a name, system prompt, provider/model selection, and allowed tools.
3. Use the Bee CLI or call `ai_agents_run()` to invoke an agent by machine name.

## Issues

Bugs and feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/ai_agents/issues).

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).

- Developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
