<?php
set_include_path(get_include_path() . PATH_SEPARATOR . 'phpseclib');

include('Crypt/Blowfish.php');

/**
 * Implement hook_rules_action_info().
 */
function sitepuller_tools_rules_action_info() {
	$actions['sitepuller_tools_action_check_remote_site'] = array (
		'label' => t('Sitepuller: Check remote site'),
		'group' => t('Sitepuller'),
		'parameter' => array(
			'node' =>  array(
				'type' => 'node',
				'label' => t('The Sitepuller Remote node.'),
				),
			),
			'provides' => array(
				'sitepuller_tools_message' => array(
					'type' => 'text',
					'label' => t('Message'),
					)
				),
		);
	$actions['sitepuller_tools_action_check_local_site'] = array (
		'label' => t('Sitepuller: Check local site'),
		'group' => t('Sitepuller'),
		'parameter' => array(
			'node' =>  array(
				'type' => 'node',
				'label' => t('The Sitepuller Remote node.'),
				),
			),
			'provides' => array(
				'sitepuller_tools_message' => array(
					'type' => 'text',
					'label' => t('Message'),
					)
				),
		);
	$actions['sitepuller_tools_action_save_task'] = array (
			'label' => t('Sitepuller: Save task'),
			'group' => t('Sitepuller'),
			'parameter' => array(
				'node' => array(
					'type' => 'node',
					'label' => t('The Sitepuller Task node.'),
					),
				),
			'provides' => array(
				'sitepuller_tools_message' => array(
					'type' => 'text',
					'label' => t('Message'),
					)
				),
		);

	return $actions;
}

/**
 * encrypt a plain text string, returning a hex string
 * @param  string $raw_string text you want to encrypt
 * @return string             hex encoded encrypted string
 */
function _sitepuller_tools_encrypt_string($raw_string) {
	$entropy = variable_get('sitepuller_tools_entropy_file', '');

	if (($entropy_file = file_get_contents($entropy)) === FALSE) {
		throw new Exception(t("can't read contents of entropy file"));
	}
	$hash = sha1($entropy_file);
	$cipher = new Crypt_Blowfish();
	$cipher->setKey($hash);
  $encrypted = $cipher->encrypt($raw_string);
  $unpacked = bin2hex($encrypted);
  return $unpacked;
}

/**
 * decrypt a hex encoded encrypted string
 * @param  string $hex_string hex string output by _sitepuller_tools_encrypt_string
 * @return string             original $raw_string input
 */
function _sitepuller_tools_decrypt_string($hex_string) {
	$entropy = variable_get('sitepuller_tools_entropy_file', '');

	if (($entropy_file = file_get_contents($entropy)) === FALSE) {
		throw new Exception(t("can't read contents of entropy file"));
	}
	$hash = sha1($entropy_file);
	$cipher = new Crypt_Blowfish();
	$cipher->setKey($hash);
	$packed = pack("H*" , $hex_string);
	$decrypted = $cipher->decrypt($packed);
	return $decrypted;
}

function _sitepuller_tools_remote_site_check($node) {
	$remote = _sitepuller_tools_info_remote($node->nid);

	_sitepuller_tools_cache_remote_site($node->nid, $remote['site_name'], $remote['host']);
	return t("Validated remote site '@name'", array('@name' => $remote['site_name']));
}

function sitepuller_tools_action_check_remote_site($node) {
	try {
		_sitepuller_tools_check_user();
		$sitepuller_tools_message[] = _sitepuller_tools_remote_site_check($node);
	}
	catch (Exception $e) {
		$sitepuller_tools_message[] = $e->getMessage();
	}
	return array (
		'sitepuller_tools_message' => implode("\r",$sitepuller_tools_message),
		);
}

function _sitepuller_tools_local_site_check($node) {
	$local = _sitepuller_tools_info_local($node->nid);

	$name = $local['site_name'];
	$tmp = $local['tmp'];
	$path = $local['site_root'];
	$use_make = $local['use_make_file'];
	$make_file = $local['drush_make_file'];

	if ($use_make != 0) {
		$cmd[] = "test -d $path || drush make --prepare-install --working-copy $make_file $path";
		$output = _sitepuller_tools_local_cmds($cmd, t('drush make failed: @dir', array('@dir' => $make_file)));
	}

	unset($cmd);
	$cmd[] = "test -d $path";
	$output = _sitepuller_tools_local_cmds($cmd, t('directory does not exist: @dir', array('@dir' => $path)));
	unset($cmd);
	$cmd[] = "cd $path";
	$cmd[] = "drush status | grep Drupal";
	$output = _sitepuller_tools_local_cmds($cmd); //, t('Can not connect to the database for site \'@name\'', array('@name' => $name)));
	$directory = "test";
	_sitepuller_tools_prepare_local_temp($tmp, $name, $directory);
	_sitepuller_tools_cache_local_site($node->nid, $name);
	return t("Validated local site '@name'", array('@name' => $name));
}

function sitepuller_tools_action_check_local_site($node) {
	try {
		_sitepuller_tools_check_user();
		$sitepuller_tools_message[] = _sitepuller_tools_local_site_check($node);
	}
	catch (Exception $e) {
		$sitepuller_tools_message[] = $e->getMessage();
	}
	return array (
		'sitepuller_tools_message' => implode("\r",$sitepuller_tools_message),
		);
}

function _sitepuller_tools_prepare_local_temp($tmp, $site, $directory = FALSE) {
	$user = variable_get('sitepuller_tools_user', 'no_user');
	$path = $directory ? "$tmp/sitepuller/$site/$user/$directory" : "$tmp/sitepuller/$site/$user";
	$cmd[] = "mkdir -p $path";
	$cmd[] = "test -d $path";
	_sitepuller_tools_local_cmds($cmd, t('Failed to create local directory: @d', array('@d' => $path)));
	return $path;
}

function _sitepuller_tools_task_check($node) {
	$sitepuller_tools_message = array();
	_sitepuller_tools_check_user();
	$task_wrapper = entity_metadata_wrapper('node', $node);
	$remote_site_nid = $task_wrapper->field_sitepuller_task_remo_site->getIdentifier();
	$remote_site = node_load($remote_site_nid);
	array_push($sitepuller_tools_message, _sitepuller_tools_remote_site_check($remote_site));
	$local_site_nid = $task_wrapper->field_sitepuller_task_local_site->getIdentifier();
	$local_site = node_load($local_site_nid);
	$local_site_wrapper = entity_metadata_wrapper('node', $local_site);
	array_push($sitepuller_tools_message, _sitepuller_tools_local_site_check($local_site));
	return $sitepuller_tools_message;
}

function sitepuller_tools_action_save_task($node) {
	try {
		_sitepuller_tools_task_check($node);
	}
	catch (Exception $e) {
		$sitepuller_tools_message[] = $e->getMessage();
		return array (
			'sitepuller_tools_message' => implode("\r ",$sitepuller_tools_message),
			);
	}
}

function _sitepuller_tools_local_cmds($cmds, $error_message = FALSE) {
	$output = array();
	$status = 0;
	$one_cmd = implode(" ; ", $cmds);
	$out = exec($one_cmd, $output, $status);
	if ($status != 0) {
		if ($error_message === FALSE) {
			$error_message = t("Error on cmd: '@step' ; exit '@exit'",
				array('@step' => $one_cmd, '@exit' => $status));
		}
		throw new Exception($error_message);
		}
	return $output;
}

/**
 * encrypt a string, return hex encoded version of encryption
 * @param  string $enropy     file contents of key file
 * @param  string $passphrase any string
 * @return string             hex encoded encrypted string
 */
function _sitepuller_tools_encrypt($entropy, $passphrase) {
  $hash = sha1($entropy);  // returns 40 character hex
  $cipher = new Crypt_Blowfish();
  // keys can range in length from 32 bits to 448 in steps of 8
  $cipher->setKey($hash);
  $encrypted = $cipher->encrypt($passphrase);
  $unpacked = bin2hex($encrypted);
  return $unpacked;
}

/**
 * quick check to see if user is configured
 * @return nothing throws exception if there is no user configuration
 */
function _sitepuller_tools_check_user() {
	$user = variable_get('sitepuller_tools_user', '');
	if (empty($user)) {
			throw new Exception(t("Please go to @path to configure your local user account for Sitepuller.",
				array('@path' => 'admin/settings/sitepuller_tools',)
				)
			);
	}
}

function _sitepuller_tools_cache_remote_site($nid, $name, $mach) {
	$cache = _sitepuller_tools_get_cached_data();
	$cache['remote']["$nid"] =  array('name' => $name, 'machine' => $mach);
	_sitepuller_tools_set_cached_data($cache);
}

function _sitepuller_tools_cache_local_site($nid, $name) {
	$cache = _sitepuller_tools_get_cached_data();
	$cache['local']["$nid"] =  array('name' => $name);
	_sitepuller_tools_set_cached_data($cache);
}

function _sitepuller_tools_get_cached_data($new_data = FALSE) {
	if ($new_data) {
		cache_set('sitepuller_tools_data', $new_data, 'cache');
		$my_data = $new_data;
	}
	else {
		$my_data = &drupal_static(__FUNCTION__);
		if (!isset($my_data)) {
			if ($cache = cache_get('sitepuller_tools_data')) {
				$my_data = $cache->data;
			}
			else {
				$my_data = array();
				cache_set('sitepuller_tools_data', $my_data, 'cache');
			}
		}
	}
	return $my_data;
}

function _sitepuller_tools_set_cached_data($my_data) {
	$new_data = _sitepuller_tools_get_cached_data($my_data);
}

function _sitepuller_tools_info_task($task_nid) {
	$info = array();
	$node = node_load($task_nid);
	$task_wrapper = entity_metadata_wrapper('node', $node);
	$info['local_db_connect'] =  $task_wrapper->field_sitepuller_loc_db_con->value();
	$info['local_site_nid'] = $task_wrapper->field_sitepuller_task_local_site->getIdentifier();
	$info['local_subsite'] =  $task_wrapper->field_sitepuller_task_local_sub->value();
	$info['private_files_path'] =  $task_wrapper->field_sitepuller_private_files->value();
	$info['public_files_path'] =  $task_wrapper->field_sitepuller_public_files->value();
	$info['pull_code'] =  ($task_wrapper->field_sitepuller_pull_code->value() == 'yes') ? 1 : 0;
	$info['pull_database'] =  ($task_wrapper->field_sitepuller_pull_database->value() == 'yes') ? 1 : 0;
	$info['pull_drupal'] =  ($task_wrapper->field_sitepuller_pull_drupal->value() == 'yes') ? 1 : 0;
	$info['pull_files'] =  ($task_wrapper->field_sitepuller_pull_files->value() == 'yes') ? 1 : 0;
	$info['pull_private'] =  ($task_wrapper->field_sitepuller_pull_private->value() == 'yes') ? 1 : 0;
	$info['remote_db_connect'] =  $task_wrapper->field_sitepuller_remo_db_con->value();
	$info['remote_site_nid'] = $task_wrapper->field_sitepuller_task_remo_site->getIdentifier();
	$info['remote_subsite'] =  $task_wrapper->field_sitepuller_task_remo_sub->value();
	return $info;
}

function _sitepuller_tools_info_remote($remote_site_nid) {
	$info = array();
	$node = node_load($remote_site_nid);
	$wrapper = entity_metadata_wrapper('node', $node);
	$info['host'] = $wrapper->field_sitepuller_remote_host->value();
	$info['root'] = $wrapper->field_sitepuller_remote_root->value();
	$info['site_name'] = $wrapper->field_sitepuller_remote_site->value();
	$info['tmp'] = $wrapper->field_sitepuller_remote_tmp->value();
	$info['user'] = $wrapper->field_sitepuller_remote_user->value();

	// keep these in this order!
	$info['stash_name'] = 'stash';
	$info['stash_dir'] = $info['tmp'] . '/sitepuller/' . $info['site_name'];
	$info['stash'] =  $info['stash_dir'] . '/' . $info['stash_name'];
	$info['tarball_name'] = $info['stash_name'] . ".tar.gz";
	$info['tarball'] = $info['stash_dir'] . '/' . $info['tarball_name'];
	return $info;
	}

function _sitepuller_tools_info_local($local_site_nid) {
	$info = array();
	$node = node_load($local_site_nid);
	$wrapper = entity_metadata_wrapper('node', $node);
	$info['drush_make_file'] = $wrapper->field_sitepuller_drush_make_file->value();
	$info['site_name'] = $wrapper->field_sitepuller_local_site_name->value();
	$info['site_root'] = $wrapper->field_sitepuller_local_site_root->value();
	$info['tmp'] = $wrapper->field_sitepuller_local_tmp->value();
	$info['use_make_file'] = ($wrapper->field_sitepuller_use_make_file->value() == 'yes') ? 1 : 0;

	// keep these in this order!
	$info['stash_name'] = 'stash';
	$info['stash_dir'] = $info['tmp'] . '/sitepuller/' . $info['site_name'];
	$info['stash'] =  $info['stash_dir'] . '/' . $info['stash_name'];
	$info['tarball_name'] = $info['stash_name'] . ".tar.gz";
	$info['tarball'] = $info['stash_dir'] . '/' . $info['tarball_name'];
	return $info;
}

