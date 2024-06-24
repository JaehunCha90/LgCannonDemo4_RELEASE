# LgCannonDemo4

## Server on Raspberry Pi
* Raspberry id/pw: lg/ lg (Not changed)
* Source Code: Winner/DemoCannon/
* How to use: Auto Run
* How to build:
  ```
  make
  ```
* Execution File: ``~/Winner/DemoCannon/DemoCannon``
* Needed files to run:
  * Path: Winner/DemoCannon/ 
  * ``Correct.ini``, ``logrotate.conf``, ``server.key``, ``logrotate.status``, ``server.crt``    

## Client on Windows

* Source Code: LgClientDisplay/
* Needed files:
  * LgClientDisplay/opencv_world490.dll
  * LgClientDisplay/opencv_world490d.dll
  * LgClientDisplay/libcrypto-3-x64.dll
  * LgClientDisplay/libssl-3-x64.dll
       
   >These files exist in the **root/Documents** path on the Raspberry Pi.

## User Database
* Login Information
  * username : Winner
  * password : 012345678(
* Login Input Validation
  * username
    * Must be less than 10 characters long
    * Can contain letters (a-z, A-Z), numbers (0-9) only.
  * password
    * Must be between 10 to 20 characters long.
    * Should include at least one number (0-9), and one special character.
* DB File : Winner/Database/user.db
* User Table Schema   
  ``
  username : Username
  ``   
  ``
  password : Hashed password
  ``   
  ``
  created_at : Date of last password change
  ``   
  ``
  is_admin : Administrator privilege
  ``
* User Database Management Program - MakeData
  * Source Code: Database/
  * How to use:
  ```
  # for build
  make

  # for execute
  ./MakeData

  # commands
  q : quit
  t : create table
  i : insert user values(username, password)
  c : insert user values(username, password, created_at)
  p : change password
  s : view all users
  d : delete user
  ```

## Additional configuration

### Download submodule repositories

  * LCCV
  * ssd1306_linux

  ```
  # for clone
  git submodule update --init --remote --recursive

  # for update
  git submodule update --recursive
  ```


 ### setup crontab for logrotate
   * crontab -e
   ```
   * * * * * /usr/sbin/logrotate ${PATH_BASE_DIR}/LgCannonDemo4/DemoCannon/logrotate.conf --state ${PATH_BASE_DIR}/LgCannonDemo4/DemoCannon/logrotate.status
   ```

