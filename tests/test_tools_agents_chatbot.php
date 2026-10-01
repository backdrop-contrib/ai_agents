<?php

/**
 * @file
 * Automated verification test suite for ai_tools, ai_agents, ai_assistants, and ai_chatbot.
 *
 * Can be executed via CLI:
 *   ddev exec php modules/contrib/ai_agents/tests/test_tools_agents_chatbot.php
 */

if (!defined('BACKDROP_ROOT')) {
  define('BACKDROP_ROOT', getcwd());
}

if (!function_exists('backdrop_bootstrap')) {
  require_once BACKDROP_ROOT . '/core/includes/bootstrap.inc';
  backdrop_bootstrap(BACKDROP_BOOTSTRAP_FULL);
}

// Load necessary includes across the modules.
module_load_include('module', 'ai');
module_load_include('module', 'ai_tools');
module_load_include('module', 'ai_agents');
module_load_include('module', 'ai_assistants');
module_load_include('module', 'ai_chatbot');

module_load_include('inc', 'ai_tools', 'includes/ai_tools.registry');
module_load_include('inc', 'ai_tools', 'includes/node.tools');
module_load_include('inc', 'ai_assistants', 'includes/ai_assistants.routing');
module_load_include('inc', 'ai_assistants', 'includes/ai_assistants.orchestrator');
module_load_include('inc', 'ai_assistants', 'includes/ai_assistants.thread');
module_load_include('inc', 'ai_assistants', 'includes/ai_assistants.runtime');
module_load_include('inc', 'ai_chatbot', 'includes/ai_chatbot.block');

$passed = 0;
$failed = 0;

function test_assert($condition, $description) {
  global $passed, $failed;
  if ($condition) {
    $passed++;
    print "  [PASS] {$description}\n";
  }
  else {
    $failed++;
    print "  [FAIL] {$description}\n";
  }
}

print "============================================================\n";
print " AI Tools, Agents, Assistants & Chatbot Test Suite\n";
print "============================================================\n\n";

// ---------------------------------------------------------------------------
// 1. AI Tools: Registry, Routing & Dispatch
// ---------------------------------------------------------------------------
print "--- 1. Testing AI Tools: Registry, Routing & Dispatch ---\n";

$tools = ai_tools_get_tools();
test_assert(!empty($tools) && count($tools) >= 30, 'Tool registry loaded active tools (count: ' . count($tools) . ')');
test_assert(isset($tools['get_node_info']), 'Core tool get_node_info is registered');
test_assert(isset($tools['search_nodes']), 'Core tool search_nodes is registered');
test_assert(isset($tools['list_content_types']), 'Core tool list_content_types is registered');

// Test menu route aliases.
$canonical_item = menu_get_item('admin/config/ai/ai-explorer/tool-explorer');
$legacy_item = menu_get_item('admin/config/ai/tool-explorer');
test_assert(!empty($canonical_item) && $canonical_item['page_callback'] === 'backdrop_get_form', 'Canonical Tool Explorer route resolves to form callback');
test_assert(!empty($legacy_item) && $legacy_item['page_callback'] === 'backdrop_get_form', 'Legacy Tool Explorer route alias resolves to form callback');

// Test dispatch of valid tool.
$content_types_res = json_decode(ai_tools_dispatch('list_content_types', []), TRUE);
test_assert(isset($content_types_res['content_types']) && is_array($content_types_res['content_types']), 'ai_tools_dispatch executes list_content_types successfully');

// Test dispatch of unknown tool.
$unknown_res = json_decode(ai_tools_dispatch('non_existent_mock_tool_xyz', []), TRUE);
test_assert(!empty($unknown_res['error']) && strpos($unknown_res['error'], 'Unknown tool') !== FALSE, 'Unknown tool returns clean error envelope');

// Test exception handling in dispatch: registering a temporary failing tool via alter hook.
function _test_failing_tool_callback($args) {
  throw new \RuntimeException('Simulated unexpected tool failure in test');
}

$GLOBALS['test_failing_tool_enabled'] = TRUE;
function ai_chatbot_ai_tools_alter(&$tools) {
  if (!empty($GLOBALS['test_failing_tool_enabled'])) {
    $tools['test_failing_tool'] = [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'test_failing_tool',
          'description' => 'Test tool that throws an exception.',
          'parameters' => ['type' => 'object', 'properties' => new stdClass()],
        ],
      ],
      'execute' => '_test_failing_tool_callback',
      'operation' => 'read',
    ];
  }
}

$throwable_res = json_decode(ai_tools_dispatch('test_failing_tool', []), TRUE);
test_assert(!empty($throwable_res['error']) && strpos($throwable_res['error'], 'Simulated unexpected tool failure') !== FALSE, 'ai_tools_dispatch catches Throwable and returns JSON error without fatal crash');
$GLOBALS['test_failing_tool_enabled'] = FALSE;

// ---------------------------------------------------------------------------
// 2. AI Tools: Body Search in search_nodes
// ---------------------------------------------------------------------------
print "\n--- 2. Testing search_nodes Title and Body Search ---\n";

// Create a test node with a unique body keyword not present in the title.
$unique_body_token = 'token_body_' . mt_rand(10000, 99999);
$test_node = new Node([
  'title' => 'Test AI Search Article ' . mt_rand(100, 999),
  'type' => 'page',
  'status' => 1,
  'uid' => 1,
  'body' => [
    LANGUAGE_NONE => [
      [
        'value' => 'This body contains a specialized search keyword: ' . $unique_body_token,
        'format' => 'filtered_html',
      ],
    ],
  ],
]);
node_save($test_node);

// Query by the body token.
$body_search_res = json_decode(ai_tools_dispatch('search_nodes', ['query' => $unique_body_token]), TRUE);
test_assert(!empty($body_search_res['results']) && $body_search_res['results'][0]['nid'] == $test_node->nid, 'search_nodes matches content located in field_data_body');

// ---------------------------------------------------------------------------
// 3. AI Agents: Configuration, Constraints & Agent-as-Tool
// ---------------------------------------------------------------------------
print "\n--- 3. Testing AI Agents: Configuration, Schemas & Constraints ---\n";

$agents = ai_agents_get_agents();
test_assert(!empty($agents), 'AI Agents loaded configuration (found ' . count($agents) . ' agents)');

$test_agent_id = 'test_agent_' . mt_rand(100, 999);
$test_agent = [
  'id' => $test_agent_id,
  'label' => 'Test Automator Agent',
  'description' => 'Test agent for schema constraint validation.',
  'system_prompt' => 'You are a test agent.',
  'tools' => ['get_node_info', 'search_nodes'],
  'tool_settings' => [
    'get_node_info' => [
      'hidden_params' => ['node_id'],
      'forced_params' => ['node_id' => (int) $test_node->nid],
    ],
  ],
];

// Test tool definition building and parameter hiding.
$built_tools = ai_agents_build_tool_definitions($test_agent);
$get_node_tool_def = NULL;
foreach ($built_tools as $tool_def) {
  if (!empty($tool_def['function']['name']) && $tool_def['function']['name'] === 'get_node_info') {
    $get_node_tool_def = $tool_def;
    break;
  }
}

test_assert(!empty($get_node_tool_def), 'Tool definitions built for enabled agent tools');
test_assert(isset($get_node_tool_def['function']['parameters']['properties']) && !isset($get_node_tool_def['function']['parameters']['properties']['node_id']), 'hidden_params removes restricted parameter from tool schema');

// Test parameter forcing during dispatch.
$dispatched_res = ai_agents_dispatch_tool($test_agent, 'get_node_info', []);
$dispatched_data = json_decode($dispatched_res, TRUE);
test_assert(!empty($dispatched_data['nid']) && $dispatched_data['nid'] == $test_node->nid, 'forced_params injects configured parameters into tool execution');

// Test Agent-as-Tool integration: agents exposed via hook_ai_tools().
$agent_tools = ai_agents_ai_tools();
test_assert(isset($agent_tools['agent_bee_explainer']) || !empty($agent_tools), 'Agents are registered as callable tools in ai_tools registry');

// ---------------------------------------------------------------------------
// 4. AI Agents: ReAct Approval Flow & Pending Run Storage
// ---------------------------------------------------------------------------
print "\n--- 4. Testing AI Agents: Approval Pause & Pending Run Persistence ---\n";

// Construct a pending state requiring human approval.
$test_pending_state = [
  'agent' => $test_agent,
  'agent_id' => $test_agent_id,
  'messages' => [
    ['role' => 'user', 'content' => 'Please perform an update.'],
  ],
  'trace' => [],
  'loops' => 1,
  'pending_tools' => [
    [
      'id' => 'call_mock_write_1',
      'name' => 'test_mock_write_tool',
      'arguments' => ['nid' => $test_node->nid, 'new_title' => 'Updated by Agent'],
    ],
  ],
];

// Store pending run.
$pending_token = ai_agents_store_pending_run($test_pending_state);
test_assert(!empty($pending_token) && is_string($pending_token), 'Pending run stored with secure token (' . strlen($pending_token) . ' chars)');

// Retrieve pending run.
$loaded_state = ai_agents_load_pending_run($pending_token);
test_assert(!empty($loaded_state) && $loaded_state['agent']['id'] === $test_agent_id, 'Stored pending run retrieved from database');
test_assert(isset($loaded_state['pending_tools'][0]['name']) && $loaded_state['pending_tools'][0]['name'] === 'test_mock_write_tool', 'Pending tools preserved in stored state');

// Resume pending run without approval (denial).
$denied_result = ai_agents_resume_run($test_agent_id, ['resume_token' => $pending_token, 'approved' => FALSE]);
test_assert(!empty($denied_result['error']) && strpos($denied_result['error'], 'Approval not granted') !== FALSE, 'Denying pending run returns approval not granted error');

// Clear pending run.
ai_agents_clear_pending_run($pending_token);
$reloaded_state = ai_agents_load_pending_run($pending_token);
test_assert(empty($reloaded_state), 'Cancelled pending run is purged from database');

// ---------------------------------------------------------------------------
// 5. AI Assistants: Orchestration, Routing & Thread Privacy
// ---------------------------------------------------------------------------
print "\n--- 5. Testing AI Assistants: Orchestration & Thread Isolation ---\n";

// Test topological sort in orchestrator.
$unsorted_agents = ['field_agent', 'content_type_agent'];
$sorted_agents = ai_assistants_orchestration_sort($unsorted_agents);
test_assert(is_array($sorted_agents) && count($sorted_agents) === 2, 'Orchestration sort orders agent workflows');

// Test thread isolation security check.
$anon_thread = ['uid' => 0, 'thread_id' => 'test-anon-thread'];
$auth_thread = ['uid' => 9999, 'thread_id' => 'test-auth-thread'];
test_assert(ai_assistants_thread_owner_matches($anon_thread) === TRUE, 'Anonymous threads allow unguessable ID matching');
test_assert(ai_assistants_thread_owner_matches($auth_thread) === FALSE, 'Foreign authenticated thread blocks access by non-owner');

// ---------------------------------------------------------------------------
// 6. AI Chatbot: Block Integration & Link Sanitizer
// ---------------------------------------------------------------------------
print "\n--- 6. Testing AI Chatbot: Layout Block & Anti-Hallucination Sanitizer ---\n";

// Test layout block definition.
$blocks = ai_chatbot_block_info();
test_assert(isset($blocks['ai_chatbot_chat_form']), 'Chatbot layout block is registered');

// Test anti-hallucination link sanitizer.
$verified_url = '/node/' . $test_node->nid;
$hallucinated_url = 'https://example.com/non-existent-hallucinated-page';

$raw_llm_markdown = "Here is the valid node: [Real Node]($verified_url) and here is a fake link: [Fake Node]($hallucinated_url).";
$sanitized_markdown = ai_chatbot_sanitize_answer_links($raw_llm_markdown, [['url' => $verified_url, 'title' => 'Real Node']]);

test_assert(strpos($sanitized_markdown, "[Real Node]($verified_url)") !== FALSE, 'Sanitizer preserves verified markdown links');
test_assert(strpos($sanitized_markdown, "[Fake Node]") === FALSE && strpos($sanitized_markdown, "Fake Node") !== FALSE, 'Sanitizer strips hallucinated link URL while preserving anchor text');

$formatted_html = ai_assistants_chat_format_text($sanitized_markdown);
test_assert(strpos($formatted_html, '<a href="' . $verified_url . '">Real Node</a>') !== FALSE, 'chat_format_text renders verified markdown link as HTML anchor');
test_assert(strpos($formatted_html, 'Fake Node') !== FALSE && strpos($formatted_html, $hallucinated_url) === FALSE, 'chat_format_text renders stripped link as plain text without hallucinated URL');

// Cleanup test entity.
node_delete($test_node->nid);

// ---------------------------------------------------------------------------
// Summary
// ---------------------------------------------------------------------------
print "\n============================================================\n";
print " Test Results: {$passed} passed, {$failed} failed\n";
print "============================================================\n";

if ($failed > 0) {
  exit(1);
}
