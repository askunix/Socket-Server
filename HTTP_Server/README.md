### 这是一个web server：

1、实现了get、post方法。
2、使用了多进程、一个mask主进程，N个worker工作进程，架构参考Nginx设计方法。
3、使用了epoll监听就绪文件描述符。
4、使用了fastCGI技术。参考Nginx。
5、支持通过PHP-FPM访问外部的.php文件。
6、需要安装PHP-fpm支持并配置。
7、需要PHP支持。
