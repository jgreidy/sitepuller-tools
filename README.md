# sitepuller_tools

Module to support Sitepuller Feature

Install sitepuller:
- see the instructions in git@github.com:jgreidy/sitepuller-profile.git

Configure sitepuller:
- go into the drupal site and go to admin/modules
- go to Sitepuller Tools -> Permissions and give yourself all permissions for
  - Sitepuller Local
  - Sitepuller Remote
  - Sitepuller Task
- go to admin/settings/sitepuller_tools for the user configuration
  - fill out the form describing your local user and machine
  - you only need to do this once

Create a node describing your local site:
    - enter a drush make file path in the sitepuller local site node
- go to node/add (Create Content) and create a Sitepuller Local node
  - the first time you save the node, it will create a site from your drush make file
  - got to the new site and run through the Drupal initialization to connect it to it's database
    - you need to create the database yourself with MAMP
    - DevDesktop creates the database for you
  - go to the site root directory and type this at the command line:
    drush sql-connect
  - copy the resulting mysql... stuff
  - reopen the Sitepuller Local node and paste it into the 'Database Connect String'
- whenever you save this node sitepuller confirms that it can work with this site

Create a node describing your remote site:
- go to Create Content on the drupal site and create a Sitepuller Remote node
  - this describes a site you want to copy on some remote machine
  - you must be set up to ssh to that machine without a password prompt
- whenever you save this node sitepuller confirms that it can work with this site

Create a Sitepuller Task to copy the remote site to the local one
- go to Create Content on the drupal site and create a Sitepuller Task node
- select the remote site as a source, and a local site as a destination
- choose which parts of the site to copy

