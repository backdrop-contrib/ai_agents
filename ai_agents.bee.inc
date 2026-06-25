<?php


/**
 * @file
 * Bee commands for AI agents.
 */

/**
 * Implements hook_bee_command().
 */
function ai_agents_bee_command() {
  $items = [];

  $items['agent-list'] = [
    'description' => bt('List configured AI agents.'),
    'aliases' => ['agent-ls'],
    'bootstrap' => BEE_BOOTSTRAP_FULL,
    'callback' => 'bee_ai_agents_list',
  ];

  $items['agent-run'] = [
    'description' => bt('Run an AI agent. If no message is given, starts an interactive loop.'),
    'bootstrap' => BEE_BOOTSTRAP_FULL,
    'arguments' => [
      'agent_id' => bt('Optional. The agent machine name. If omitted, a prompt lists available agents.'),
      'message' => bt('Optional. One-shot prompt. If omitted, an interactive session starts.'),
    ],
    'optional_arguments' => ['agent_id', 'message'],
    'options' => [
      'resume-token' => [
        'description' => bt('Resume token from a paused approval-required run.'),
      ],
      'approved' => [
        'description' => bt('Set to 1 to approve the paused tool call when resuming.'),
      ],
    ],
    'callback' => 'bee_ai_agents_run',
  ];

  return $items;
}

/**
 * Bee callback: list configured agents.
 */
function bee_ai_agents_list() {
  $agents = ai_agents_get_agents();
  if (empty($agents)) {
    bee_message(bt('No AI agents configured.'), 'warning');
    return;
  }

  foreach ($agents as $agent_id => $agent) {
    $label = !empty($agent['label']) ? $agent['label'] : $agent_id;
    $model = !empty($agent['model']) ? $agent['model'] : ai_agents_default_model();
    $tool_count = !empty($agent['tools']) && is_array($agent['tools']) ? count(array_filter($agent['tools'])) : 0;
    bee_message(bt('@id: @label | @model | @tools tools', [
      '@id' => $agent_id,
      '@label' => $label,
      '@model' => $model,
      '@tools' => $tool_count,
    ]));
  }
}

/**
 * Bee callback: run one AI agent.
 */
function bee_ai_agents_run($arguments = [], $options = []) {
  $agent_id = !empty($arguments['agent_id']) ? (string) $arguments['agent_id'] : '';
  $message = array_key_exists('message', $arguments) ? (string) $arguments['message'] : '';

  // Map hyphenated bee options to the keys ai_agents_run() expects.
  if (!empty($options['resume-token'])) {
    $options['resume_token'] = (string) $options['resume-token'];
    $options['approved'] = !empty($options['approved']);
    // Resuming needs no message; don't fall into the interactive loop.
    bee_ai_agents_print_run_result($agent_id, '', $options);
    return;
  }

  if ($agent_id === '') {
    $agent_id = bee_ai_agents_prompt_for_agent_id();
    if ($agent_id === '') {
      return;
    }
  }

  if ($message !== '') {
    bee_ai_agents_print_run_result($agent_id, $message, $options);
    return;
  }

  if (!function_exists('readline')) {
    bee_message(bt('Interactive input requires readline. Falling back to fgets(STDIN).'), 'warning');
  }

  while (TRUE) {
    $input = function_exists('readline') ? readline('You: ') : fgets(STDIN);
    if ($input === FALSE) {
      break;
    }

    $input = trim($input);
    if ($input === '') {
      continue;
    }
    if (in_array($input, ['exit', 'quit', 'q'], TRUE)) {
      break;
    }

    bee_ai_agents_print_run_result($agent_id, $input, $options);
  }
}

/**
 * Prompt for an agent when none is passed on the command line.
 */
function bee_ai_agents_prompt_for_agent_id() {
  $options = ai_agents_list_agent_options();
  if (empty($options)) {
    bee_message(bt('No AI agents configured.'), 'warning');
    return '';
  }

  bee_message(bt('Available agents:'), 'log');
  $index = 1;
  $agent_ids = [];
  foreach ($options as $agent_id => $label) {
    $agent_ids[] = $agent_id;
    bee_message($index . '. ' . $label . ' [' . $agent_id . ']', 'log');
    $index++;
  }

  $selected = function_exists('readline') ? readline('Select agent number: ') : fgets(STDIN);
  $selected = (int) trim((string) $selected);
  if ($selected < 1 || $selected > count($agent_ids)) {
    bee_message(bt('Invalid selection.'), 'error');
    return '';
  }

  return $agent_ids[$selected - 1];
}

/**
 * Execute one prompt and print the result.
 */
function bee_ai_agents_print_run_result($agent_id, $message, array $options = []) {
  $result = ai_agents_run($agent_id, $message, $options);
  if (is_string($result)) {
    bee_message(bt('Agent returned an async job id: @id', ['@id' => $result]), 'log');
    return;
  }

  if (!empty($result['error'])) {
    bee_message(bt('Error: @msg', ['@msg' => $result['error']]), 'error');
    return;
  }

  if (!empty($result['status']) && $result['status'] === 'needs_approval') {
    $tool_name = !empty($result['pending_tools'])
      ? ai_agents_pending_tools_summary($result['pending_tools'], bt('unknown'))
      : (!empty($result['pending_tool']['name']) ? $result['pending_tool']['name'] : bt('unknown'));
    bee_message(bt('Approval needed for tool(s): @tool — approving runs ALL of them.', ['@tool' => $tool_name]), 'warning');
    if (!empty($result['token'])) {
      bee_message(bt('Resume token: @token', ['@token' => $result['token']]), 'log');
      bee_message(bt('Resume with: bee agent-run @agent_id --resume-token=@token --approved=1', [
        '@agent_id' => $agent_id,
        '@token' => $result['token'],
      ]), 'log');
    }
    return;
  }

  $answer = isset($result['answer']) ? trim((string) $result['answer']) : '';
  if ($answer === '') {
    bee_message(bt('The agent did not return a response. The model may not support this request or the max loop limit was reached.'), 'warning');
    return;
  }

  print $answer . "\n";
}
