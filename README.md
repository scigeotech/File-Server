# File-Server
Creates a server with port 8000, and can serve files within the requested directory.
After compiling (any file name works but I use "Server"), use with ./Server (file path)

For instance, "./Server ." will serve files from the current directory.
Then, in a browser, go to http://localhost:8000/data.html to open the "data.html" file within the directory.
In the case that data.html is in the "files" folder within the directory, go to http://localhost:8000/files/data.html.