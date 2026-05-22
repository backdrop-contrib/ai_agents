<?php


/**
 * @file
 * Admin forms for AI Agent.
 */

/**
 * List page.
 */
function ai_agents_admin_list_form($form, &$form_state) {
  $agents = ai_agents_get_agents();
  $form['agents'] = [
    '#type' => 'fieldset',
    '#title' => t('Configured agents'),
  ];

  if (empty($agents)) {
    $form['agents']['empty'] = [
      '#markup' => '<p>' . t('No agents configured yet.') . '</p>',
    ];
  }
  else {
    $rows = [];
    foreach ($agents as $id => $agent) {
      $rows[] = [
        check_plain($id),
        check_plain(isset($agent['label']) ? $agent['label'] : $id),
        check_plain(isset($agent['model']) ? $agent['model'] : ''),
        check_plain(implode(', ', array_values(array_filter(isset($agent['tools']) ? $agent['tools'] : [])))),
        l(t('Edit'), 'admin/config/ai/ai-agents/' . $id . '/edit') . ' | ' . l(t('Delete'), 'admin/config/ai/ai-agents/' . $id . '/delete'),
      ];
    }
    $form['agents']['table'] = [
      '#theme' => 'table',
      '#header' => [t('ID'), t('Label'), t('Model'), t('Tools'), t('Operations')],
      '#rows' => $rows,
      '#empty' => t('No agents configured yet.'),
    ];
  }

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['add'] = [
    '#type' => 'link',
    '#title' => t('Add agent'),
    '#href' => 'admin/config/ai/ai-agents/add',
    '#attributes' => ['class' => ['button', 'button-primary']],
  ];
  if (module_exists('ai_agents_maintenance')) {
    $form['actions']['maintenance'] = [
      '#type' => 'link',
      '#title' => t('Maintenance'),
      '#href' => 'admin/config/ai/ai-agents/maintenance',
      '#attributes' => ['class' => ['button']],
    ];
  }

  return $form;
}

/**
 * Edit/add form.
 */
function ai_agents_admin_edit_form($form, &$form_state, $agent_id = '') {
  $agents = ai_agents_get_agents();
  $is_edit = !empty($agent_id) && isset($agents[$agent_id]);
  $agent = $is_edit ? $agents[$agent_id] : [];
  $tools = ai_tools_get_tools();

  // The human-readable label is used as the source for the machine name
  // preview, so ensure it is defined before the machine_name element.
  $form['label'] = [
    '#type' => 'textfield',
    '#title' => t('Label'),
    '#default_value' => isset($agent['label']) ? $agent['label'] : '',
    '#required' => TRUE,
  ];

  $form['agent_id'] = [
    '#type' => 'machine_name',
    '#title' => t('Agent ID'),
    '#default_value' => $is_edit ? $agent_id : '',
    '#machine_name' => [
      'exists' => 'ai_agents_admin_agent_exists',
      'source' => ['label'],
    ],
    '#required' => TRUE,
    '#disabled' => $is_edit,
  ];


  $form['description'] = [
    '#type' => 'textarea',
    '#title' => t('Description'),
    '#default_value' => isset($agent['description']) ? $agent['description'] : '',
    '#rows' => 2,
  ];

  $form['system_prompt'] = [
    '#type' => 'textarea',
    '#title' => t('System prompt'),
    '#default_value' => isset($agent['system_prompt']) ? $agent['system_prompt'] : '',
    '#rows' => 8,
    '#required' => TRUE,
  ];

  $form['secured_system_prompt'] = [
    '#type' => 'textarea',
    '#title' => t('Secured system prompt'),
    '#default_value' => isset($agent['secured_system_prompt']) ? $agent['secured_system_prompt'] : '',
    '#rows' => 4,
    '#description' => t('Injected after the user message so it is harder to override.'),
  ];

  $form['model'] = [
    '#type' => 'select',
    '#title' => t('Model'),
    '#options' => ai_get_all_chat_models(),
    '#default_value' => isset($agent['model']) ? $agent['model'] : ai_agents_default_model(),
    '#required' => TRUE,
  ];

  $form['temperature'] = [
    '#type' => 'number',
    '#title' => t('Temperature'),
    '#default_value' => isset($agent['temperature']) ? $agent['temperature'] : 0.3,
    '#min' => 0,
    '#max' => 2,
    '#step' => 0.1,
  ];

  $form['max_tokens'] = [
    '#type' => 'number',
    '#title' => t('Max tokens'),
    '#default_value' => isset($agent['max_tokens']) ? $agent['max_tokens'] : 1024,
    '#min' => 1,
    '#step' => 1,
  ];

  $form['max_loops'] = [
    '#type' => 'number',
    '#title' => t('Max loops'),
    '#default_value' => isset($agent['max_loops']) ? $agent['max_loops'] : 5,
    '#min' => 1,
    '#step' => 1,
    '#description' => t('Hard stop for tool-call loops.'),
  ];

  $form['orchestration'] = [
    '#type' => 'fieldset',
    '#title' => t('Orchestration'),
    '#description' => t('Used by ai_assistants when this agent runs alongside others. Lower priority runs first; declared dependencies are enforced after the LLM router picks agents.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  ];

  $form['orchestration']['priority'] = [
    '#type' => 'number',
    '#title' => t('Priority'),
    '#default_value' => isset($agent['priority']) ? (int) $agent['priority'] : 100,
    '#min' => 0,
    '#step' => 1,
    '#description' => t('Sort hint within an ask_many run. Lower numbers run earlier. Default 100.'),
  ];

  $other_agents_options = [];
  $current_agents = ai_agents_get_agents();
  foreach ($current_agents as $id => $other) {
    if ($is_edit && $id === $agent_id) {
      continue;
    }
    $other_agents_options[$id] = (!empty($other['label']) ? $other['label'] : $id) . ' (' . $id . ')';
  }
  $form['orchestration']['depends_on'] = [
    '#type' => 'checkboxes',
    '#title' => t('Depends on'),
    '#options' => $other_agents_options,
    '#default_value' => isset($agent['depends_on']) && is_array($agent['depends_on']) ? array_values($agent['depends_on']) : [],
    '#description' => t('When both this agent and a checked agent are part of the same plan, the checked agent must run first. Used for context piping (produces/consumes callbacks declared in code).'),
  ];

  $form['orchestration']['routing_usage_instructions'] = [
    '#type' => 'textarea',
    '#title' => t('Router usage instructions'),
    '#default_value' => isset($agent['routing_usage_instructions']) ? $agent['routing_usage_instructions'] : '',
    '#rows' => 3,
    '#description' => t('Free-text guidance to teach the assistant\'s router when this agent should or should not be selected. Substituted into the [usage_instructions] placeholder of the pre-action prompt at routing time. Example: "Use this agent only for taxonomy work. Do not pick it for content type or field requests."'),
  ];

  $form['orchestration']['routing_examples'] = [
    '#type' => 'textarea',
    '#title' => t('Router few-shot examples'),
    '#default_value' => isset($agent['routing_examples']) ? $agent['routing_examples'] : '',
    '#rows' => 6,
    '#description' => t('Free-text examples showing user requests that should route to THIS agent. Substituted into the [examples] placeholder of the pre-action prompt at routing time. Example: <code>User: "create a vocabulary called Tags" → {"strategy":"choose_one","agent_ids":["taxonomy_agent"],"reason":"only this agent has modify_vocabulary"}</code>'),
  ];

  $form['tools'] = [
    '#type' => 'checkboxes',
    '#title' => t('Enabled tools'),
    '#options' => ai_agents_group_tools($tools),
    '#default_value' => isset($agent['tools']) && is_array($agent['tools']) ? array_values($agent['tools']) : [],
    '#description' => t('Only selected tools are exposed to this agent.'),
  ];

  $approval_tools = ai_agents_get_agent_approval_tools($agent, $tools);
  if (!empty($approval_tools)) {
    $form['approval_warning'] = [
      '#type' => 'item',
      '#title' => t('Approval required'),
      '#markup' => '<div class="messages warning">' . t('This agent includes tools that will pause for approval before execution: @tools.', [
        '@tools' => implode(', ', array_map('check_plain', $approval_tools)),
      ]) . '</div>',
    ];
  }

  $rag_tools_enabled = !empty($agent['tools']) && is_array($agent['tools']) && in_array('ai_search_rag_search', array_values(array_filter($agent['tools'])), TRUE);
  if ($rag_tools_enabled && module_exists('ai_search') && function_exists('ai_search_list_rag_indexes')) {
    $available_indexes = ai_search_list_rag_indexes();
    $index_options = [];
    if (!empty($available_indexes['indexes']) && is_array($available_indexes['indexes'])) {
      foreach ($available_indexes['indexes'] as $index_info) {
        if (empty($index_info['index_id'])) {
          continue;
        }
        $label = !empty($index_info['label']) ? $index_info['label'] : $index_info['index_id'];
        $backend = !empty($index_info['backend']) ? ' (' . $index_info['backend'] . ')' : '';
        $index_options[$index_info['index_id']] = $label . $backend;
      }
    }

    $saved_index_ids = [];
    if (!empty($agent['tool_settings']['ai_search_rag_search']['forced_params']['index_ids']) && is_array($agent['tool_settings']['ai_search_rag_search']['forced_params']['index_ids'])) {
      $saved_index_ids = array_values(array_filter(array_map('strval', $agent['tool_settings']['ai_search_rag_search']['forced_params']['index_ids'])));
    }

    $form['rag_search_settings'] = [
      '#type' => 'fieldset',
      '#title' => t('RAG search defaults'),
      '#description' => t('These defaults are applied to ai_search_rag_search for this agent. Use them to pin different agents to different vector indexes.'),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
    ];
    $form['rag_search_settings']['rag_index_ids'] = [
      '#type' => 'checkboxes',
      '#title' => t('Default index IDs'),
      '#description' => t('Select the vector indexes this agent should search by default. If none are selected, the model must supply index_ids itself.'),
      '#options' => $index_options,
      '#default_value' => $saved_index_ids,
    ];
  }

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Save'),
  ];

  return $form;
}

/**
 * Delete form.
 */
function ai_agents_admin_delete_form($form, &$form_state, $agent_id = '') {
  $agents = ai_agents_get_agents();
  if (empty($agent_id) || !isset($agents[$agent_id])) {
    return ['#markup' => '<p>' . t('Agent not found.') . '</p>'];
  }

  $form['agent_id'] = ['#type' => 'value', '#value' => $agent_id];
  $form['label'] = ['#type' => 'value', '#value' => isset($agents[$agent_id]['label']) ? $agents[$agent_id]['label'] : $agent_id];
  return confirm_form(
    $form,
    t('Delete agent %label?', ['%label' => isset($agents[$agent_id]['label']) ? $agents[$agent_id]['label'] : $agent_id]),
    'admin/config/ai/ai-agents',
    t('This removes the agent config only.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Save handler.
 */
function ai_agents_admin_edit_form_submit($form, &$form_state) {
  $agents = ai_agents_get_agents();
  $agent_id = trim((string) $form_state['values']['agent_id']);
  $depends_on = [];
  if (!empty($form_state['values']['depends_on']) && is_array($form_state['values']['depends_on'])) {
    $depends_on = array_values(array_filter(array_map('strval', $form_state['values']['depends_on'])));
  }
  $priority = isset($form_state['values']['priority']) && is_numeric($form_state['values']['priority'])
    ? (int) $form_state['values']['priority']
    : 100;

  $agents[$agent_id] = [
    'label' => trim((string) $form_state['values']['label']),
    'description' => trim((string) $form_state['values']['description']),
    'system_prompt' => trim((string) $form_state['values']['system_prompt']),
    'secured_system_prompt' => trim((string) $form_state['values']['secured_system_prompt']),
    'model' => trim((string) $form_state['values']['model']),
    'temperature' => (float) $form_state['values']['temperature'],
    'max_tokens' => (int) $form_state['values']['max_tokens'],
    'max_loops' => (int) $form_state['values']['max_loops'],
    'tools' => array_values(array_filter($form_state['values']['tools'])),
    'priority' => $priority,
    'depends_on' => $depends_on,
    'routing_usage_instructions' => isset($form_state['values']['routing_usage_instructions']) ? trim((string) $form_state['values']['routing_usage_instructions']) : '',
    'routing_examples' => isset($form_state['values']['routing_examples']) ? trim((string) $form_state['values']['routing_examples']) : '',
  ];

  if (isset($form_state['values']['rag_index_ids']) && is_array($form_state['values']['rag_index_ids'])) {
    $rag_index_ids = array_values(array_filter($form_state['values']['rag_index_ids']));
    if (!isset($agents[$agent_id]['tool_settings']) || !is_array($agents[$agent_id]['tool_settings'])) {
      $agents[$agent_id]['tool_settings'] = [];
    }
    if (empty($rag_index_ids)) {
      unset($agents[$agent_id]['tool_settings']['ai_search_rag_search']['forced_params']['index_ids']);
      if (empty($agents[$agent_id]['tool_settings']['ai_search_rag_search']['forced_params']) && empty($agents[$agent_id]['tool_settings']['ai_search_rag_search']['hidden_params'])) {
        unset($agents[$agent_id]['tool_settings']['ai_search_rag_search']);
      }
      if (empty($agents[$agent_id]['tool_settings'])) {
        unset($agents[$agent_id]['tool_settings']);
      }
    }
    else {
      if (empty($agents[$agent_id]['tool_settings']['ai_search_rag_search']) || !is_array($agents[$agent_id]['tool_settings']['ai_search_rag_search'])) {
        $agents[$agent_id]['tool_settings']['ai_search_rag_search'] = [
          'forced_params' => [],
          'hidden_params' => [],
        ];
      }
      $agents[$agent_id]['tool_settings']['ai_search_rag_search']['forced_params']['index_ids'] = $rag_index_ids;
    }
  }
  ai_agents_save_agents($agents);
  backdrop_set_message(t('Agent saved.'));
  $form_state['redirect'] = 'admin/config/ai/ai-agents';
}

/**
 * Delete handler.
 */
function ai_agents_admin_delete_form_submit($form, &$form_state) {
  $agent_id = (string) $form_state['values']['agent_id'];
  if (ai_agents_delete($agent_id)) {
    backdrop_set_message(t('Agent deleted.'));
  }
  $form_state['redirect'] = 'admin/config/ai/ai-agents';
}

/**
 * Machine name existence callback.
 */
function ai_agents_admin_agent_exists($agent_id) {
  $agents = ai_agents_get_agents();
  return isset($agents[$agent_id]);
}

/**
 * Group tools by category for checkboxes.
 */
function ai_agents_group_tools(array $tools) {
  $grouped = [];
  foreach ($tools as $key => $tool) {
    $category = isset($tool['category']) ? $tool['category'] : 'general';
    $label = isset($tool['definition']['function']['name']) ? $tool['definition']['function']['name'] : $key;
    $desc = isset($tool['definition']['function']['description']) ? $tool['definition']['function']['description'] : '';
    $grouped[$category][$key] = check_plain($label) . ' — ' . check_plain($desc);
  }
  ksort($grouped);
  $flat = [];
  foreach ($grouped as $items) {
    $flat += $items;
  }
  return $flat;
}

/**
 * Form builder: Agent Explorer.
 */
function ai_agents_explorer_form($form, &$form_state) {
  $agents = ai_agents_get_agents();

  if (empty($agents)) {
    $form['no_agents'] = [
      '#markup' => '<p class="messages warning">' . t('No agents configured. <a href="@url">Add an agent</a> first.', ['@url' => url('admin/config/ai/ai-agents/add')]) . '</p>',
    ];
    return $form;
  }

  $agent_options = [];
  foreach ($agents as $id => $agent) {
    $agent_options[$id] = !empty($agent['label']) ? $agent['label'] . ' (' . $id . ')' : $id;
  }

  $last_agent = !empty($form_state['storage']['last_agent']) ? $form_state['storage']['last_agent'] : key($agent_options);

  $form['agent_id'] = [
    '#type' => 'select',
    '#title' => t('Agent'),
    '#options' => $agent_options,
    '#default_value' => $last_agent,
    '#required' => TRUE,
  ];

  $form['message'] = [
    '#type' => 'textarea',
    '#title' => t('Message'),
    '#rows' => 3,
    '#default_value' => !empty($form_state['storage']['last_message']) ? $form_state['storage']['last_message'] : '',
    '#required' => TRUE,
    '#description' => t('The user message sent to the agent.'),
  ];

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Run agent'),
  ];

  if (!empty($form_state['storage']['result'])) {
    $result = $form_state['storage']['result'];

    $form['results'] = [
      '#type' => 'fieldset',
      '#title' => t('Run trace — @loops loop(s)', ['@loops' => $result['loops']]),
      '#weight' => 10,
    ];

    if (!empty($result['error'])) {
      $form['results']['error'] = [
        '#markup' => '<div class="messages error">' . check_plain($result['error']) . '</div>',
      ];
    }

    if (!empty($result['status']) && $result['status'] === 'needs_approval') {
      $tool_name = !empty($result['pending_tool']['name']) ? $result['pending_tool']['name'] : t('a write tool');
      $form['results']['approval_notice'] = [
        '#markup' => '<div class="messages warning">'
          . t('The agent paused before executing <strong>@tool</strong> because it requires approval. The Explorer is read-only — no changes were made to the site. To execute write operations, use the Assistant interface.', ['@tool' => check_plain($tool_name)])
          . '</div>',
        '#weight' => -10,
      ];
    }

    foreach ($result['tool_calls'] as $i => $step) {
      if (!empty($step['loop'])) {
        $label = t('Loop @n — model (@reason)', ['@n' => $step['loop'], '@reason' => $step['finish_reason']]);
        $body = '';
        if (!empty($step['content'])) {
          $body .= t('Content:') . "\n" . check_plain($step['content']) . "\n\n";
        }
        if (!empty($step['tool_calls'])) {
          $body .= t('Tool calls:') . "\n" . check_plain(json_encode($step['tool_calls'], JSON_PRETTY_PRINT));
          if (!empty($result['status']) && $result['status'] === 'needs_approval') {
            $body .= "\n\n" . t('(paused — write tool requested, not executed)');
          }
        }
      }
      else {
        $label = t('Tool: @name', ['@name' => check_plain($step['tool'])]);
        $body = t('Args:') . "\n" . check_plain(json_encode($step['args'], JSON_PRETTY_PRINT))
          . "\n\n" . t('Result:') . "\n" . check_plain(is_string($step['result']) ? $step['result'] : json_encode($step['result'], JSON_PRETTY_PRINT));
      }

      $form['results']['step_' . $i] = [
        '#type' => 'fieldset',
        '#title' => $label,
        '#collapsible' => TRUE,
        '#collapsed' => FALSE,
      ];
      $form['results']['step_' . $i]['body'] = [
        '#markup' => '<pre>' . $body . '</pre>',
      ];
    }

    if (!empty($result['answer'])) {
      $form['results']['answer'] = [
        '#type' => 'fieldset',
        '#title' => t('Final answer'),
        '#collapsible' => FALSE,
        '#weight' => 99,
      ];
      $form['results']['answer']['text'] = [
        '#markup' => '<div>' . nl2br(check_plain($result['answer'])) . '</div>',
      ];
    }
  }

  return $form;
}

/**
 * Submit handler: Agent Explorer.
 */
function ai_agents_explorer_form_submit($form, &$form_state) {
  $agent_id = (string) $form_state['values']['agent_id'];
  $message = trim((string) $form_state['values']['message']);

  $form_state['storage']['last_agent'] = $agent_id;
  $form_state['storage']['last_message'] = $message;

  $agent = ai_agents_load($agent_id);
  if (empty($agent)) {
    $form_state['storage']['result'] = [
      'answer' => '',
      'loops' => 0,
      'tool_calls' => [],
      'error' => 'Agent not found: ' . $agent_id,
    ];
    $form_state['rebuild'] = TRUE;
    return;
  }

  $agent['agent_id'] = $agent_id;
  $result = ai_agents_run_sync($agent, $message, ['operation' => 'agent_explorer']);
  $form_state['storage']['result'] = $result;
  $form_state['rebuild'] = TRUE;
}

/**
 * Returns enabled tools that will require approval.
 */
function ai_agents_get_agent_approval_tools(array $agent, array $tools) {
  $enabled = !empty($agent['tools']) && is_array($agent['tools']) ? array_values(array_filter($agent['tools'])) : [];
  if (empty($enabled)) {
    return [];
  }

  $approval_tools = [];
  foreach ($enabled as $tool_key) {
    if (empty($tools[$tool_key])) {
      continue;
    }
    if (ai_agents_tool_requires_approval($tools[$tool_key])) {
      $approval_tools[] = isset($tools[$tool_key]['definition']['function']['name']) ? $tools[$tool_key]['definition']['function']['name'] : $tool_key;
    }
  }

  return $approval_tools;
}
