CREATE USER 'paul'@'%' IDENTIFIED BY 'password';
GRANT ALL PRIVILEGES ON *.* TO 'paul'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
