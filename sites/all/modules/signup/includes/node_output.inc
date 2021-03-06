<?php
/**
 * @file
 * Code used to generate singup-related output when viewing nodes.
 */

/**
 * Generate all the signup-related output for a given node.
 *
 * Because of the global setting to control if the signup details and form
 * appear at the bottom of the node or on a separate tab, this function is
 * shared by multiple callers.
 *
 * @param $node
 *   The fully loaded node object.
 * @param $type
 *   The kind of output would we render: can be either 'node' or 'tab'.
 *
 * @return
 *   The fully rendered HTML for all signup-related forms and info.
 *
 * @see signup_nodeapi()
 * @see signup_node_tab()
 *
 * @todo This needs to be much more theme-friendly.
 *
 */
function _signup_node_output($node, $type = 'node') {
  $output = theme('signup_node_output_header', array('node' => $node));
  $output .= _signup_current_user_signup($node, $type);
  return $output;
}

/**
 * Helper function to generate the output for the current user's signup.
 */
function _signup_current_user_signup($node, $type = 'node') {
  global $user;
  $output = '';
  $fieldset = $type == 'node' && !variable_get('signup_ignore_default_fields', 0) ? TRUE : FALSE;
  // The node has been closed for signups, and the user has
  // signup permissions.  Let them know it's closed.
  if (!$node->signup_status) {
    if (user_access('sign up for content')) {
      $current_signup = '';
      // If they're logged in and already signed up, show their current
      // signup info and give them the option to cancel.
      if ($user->uid) {
        $signup = db_query("SELECT sl.*, n.title, u.name FROM {signup_log} sl INNER JOIN {node} n ON sl.nid = n.nid INNER JOIN {users} u ON sl.uid = u.uid WHERE sl.uid = :uid AND sl.nid = :nid", array(':uid' => $user->uid, ':nid' => $node->nid))->fetchObject();
        if (!empty($signup)) {
          $current_signup = _signup_render_signup_edit_form($signup, $type);
        }
      }
      $output .= theme('signup_signups_closed', array('node' => $node, 'current_signup' => $current_signup));
    }
  }
  else {
    if ($user->uid == 0) {
      // This is an anonymous user.
      if (user_access('sign up for content')) {
        // If they can signup, render the anonymous sigup form.
        module_load_include('inc', 'signup', 'includes/signup_form');
        $form = drupal_get_form('signup_form', $node, 'anon', $fieldset);
        $output .= drupal_render($form);
      }
      else {
        // If not, then display the appropriate login/register link if the
        // default authenticated user role can signup.
        $anon_login_text = '';
        $signup_roles = user_roles(FALSE, 'sign up for content');
        if (!empty($signup_roles[DRUPAL_AUTHENTICATED_RID])) {
          $token_array = array(
            '!login' => l(t('login'), 'user/login', array('query' => drupal_get_destination())),
            '!register' => l(t('register'), 'user/register', array('query' => drupal_get_destination())),
            '%node_type' => node_type_get_name($node->type),
          );
          if (variable_get('user_register', 1) == 0) {
            $anon_login_text = t('Please !login to sign up for this %node_type.', $token_array);
          }
          else {
            $anon_login_text = t('Please !login or !register to sign up for this %node_type.', $token_array);
          }
        }
        $output .= theme('signup_anonymous_user_login_text', array('anon_login_text' => $anon_login_text));
      }
    }
    else {
      // An authenticated user.

      // See if the user is already signed up for this node.
      $signup = db_query("SELECT sl.*, n.title, u.name, u.mail FROM {signup_log} sl INNER JOIN {node} n ON sl.nid = n.nid INNER JOIN {users} u ON sl.uid = u.uid WHERE sl.uid = :uid AND sl.nid = :nid", array(':uid' => $user->uid, ':nid' => $node->nid))->fetchObject();
      if (empty($signup)) {
        // Not yet signed up
        if (user_access('sign up for content')) {
          // User has permission to do so, so give them the form.
          module_load_include('inc', 'signup', 'includes/signup_form');
          $form = drupal_get_form('signup_form', $node, 'auth', $fieldset);
          $output .= drupal_render($form);
        }
      }
      else {
        // Already signed up, display their info.
        $output .= _signup_render_signup_edit_form($signup, $type);
      }
    }
  }
  return $output;
}

/**
 * Helper function to generate the list of users signed up for a node.
 */
function signup_user_list_output($node) {
  $output = '';

  // TODO: This probably shouldn't be here.
  drupal_set_title($node->title);

  // How should the list of signed-up users be displayed, if at all?
  $display_list = variable_get('signup_display_signup_user_list', 'embed-view');

  // Ensure the user has permission to view the signup list for this node.
  if (_signup_menu_access($node, 'list') && ($display_list == 'embed-view' || $display_list == 'embed-view-tab')) {
    $signup_view = variable_get('signup_user_list_view', 'signup_user_list:default');
    $signup_view_parts = explode(':', $signup_view);
    $view_name = $signup_view_parts[0];
    $view_display = $signup_view_parts[1];
    $view = views_get_view($view_name);
    if ($display_list == 'embed-view-tab') {
      $view->override_path = 'node/%/signups/list';
    }
    else {
      $view->override_path = 'node/%';
    }
    $view_args = array($node->nid);
    $output .=  $view->preview($view_display, $view_args);
  }
  return $output;
}

/**
 * Page handler for the optional 'signup' tab on nodes.
 *
 * This is only used if the site has configured the signup form and related
 * output to appear on a separate tab, instead of directly embedded in the
 * node.
 *
 * @param $node
 *   The node to generate a signup tab for.
 *
 * @return
 *   The contents of the signup tab.
 *
 * @see _signup_node_output()
 */
function signup_node_tab($node) {
  drupal_set_title($node->title);
  return _signup_node_output($node, 'tab');
}

/**
 * Helper function to render the form to edit your own signup.
 */
function _signup_render_signup_edit_form($signup, $type) {
  $path = drupal_get_path('module', 'signup');
  if ($type == 'node') {
    drupal_add_css("$path/signup.css");
  }
  drupal_add_js("$path/js/signup_edit_form.js");
  module_load_include('inc', 'signup', 'includes/signup_edit_form');
  $form = drupal_get_form('signup_edit_form', $signup, $type);
  $form_errors = form_get_errors() ? TRUE : FALSE;
  drupal_add_js(array('signupEditFormErrors' => $form_errors), array('type' => 'setting', 'scope' => JS_DEFAULT));
  return drupal_render($form);
}

