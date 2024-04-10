My shell - CS214 P2

[ MEMBERS ]
Colin Cunningham - cdc166
Richard Li - rl902

[ MAJOR DESIGN NOTES ]
Our program mainly revolves around 3 components. The first is the actual shell inviroment itself which is handled within the main function and the myShellInteract and myShellBatch functions. These handle the logic regarding what mode to run the shell in, using itatty to detect for changes in standrd input and also detecting if any files were given as arguments. The next component of the program is the handling of the physical commands which are read in line by line by our function readLine() and then split into managable tokens by our splitLine() function. These are then fed into the next component of our program which is the execShell() function that handles all the bits and bobs feeding into the myShell_execute() function to handle pipes and redirections while also handling the built in functions specified in our built in function list. Wild cards are also expanded here with our expand_Wildcard function. 

[ TEST PLAN ]
Our test plan was rudimentery but effective. Using 2 custom made executables echo.c and hello.c as well as a long list of .txt files, we were able to test redirection, piping, using piping and redirection together, using wild cards with redirection and piping, as well as redirecting and piping to and from multiple files. Some exsample commands were:

./hello | ./echo > *.txt

./hello | ./echo

Entering these commands into a seperate .sh file and running them in batch mode also yeided tested the capability to run in batch mode. 
