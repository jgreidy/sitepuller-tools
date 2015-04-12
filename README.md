# sitepuller_tools

Module to support Sitepuller Feature

Install sitepuller:
  - create a new Drupal 7 site on your local machine <yoursite>
  - cd <yoursite>/sites/all/modules
  - git clone git@github.com:jgreidy/sitepuller.git
  - git clone git@github.com:jgreidy/sitepuller-tools.git sitepuller_tools
  - drush pm-enable sitepuller
  - (agree to download and enable dependencies)
    > The following projects have unmet dependencies:
      sitepuller requires strongarm, rules, features, entityreference, entity, devel, ctools, composer_manager
      Would you like to download them? (y/n): y
    > The following extensions will be enabled: sitepuller, strongarm, rules_admin, entity_token,
      rules, features, entityreference, entity, devel, ctools, composer_manager
      Do you really want to continue? (y/n): y
    > One or more extensions have dependencies managed by Composer.
      Update packages managed by Composer? (y/n): y
 - some more steps

