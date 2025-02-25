#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#include <texted/edit.h>
#include <texted/print.h>
#include <texted/insert.h>
#include <texted/fileio.h>
#include <texted/texted.h>

#include <credits/credits.h>

int main(int argc, char* argv[])
{
	char* Buffer = NULL;					// Continuous buffer
	char* Filename;							// Name of open file
	LineBuffer_s* LineBuffer;				// Array of lines
	size_t Line = 1;						// Selected row
	commans_s Command;						// Command
	size_t counter;							// Global counter
	int status = 0;							// Return status
	usr_perm_e permissions;					// Operations we can perform on this file

	// LOADING
	if (argc <= 1) {
		Filename = "a.txt";
	} else if(streq(argv[1], "--help", 7) || streq(argv[1], "-h", 3)) {
		display_help();
		return 0;
	} else if(streq(argv[1], "-v", 3) || streq(argv[1], "--version", 10) ) {
		fputs(BOLD RED " / \\--------------, \n"
			  RED " \\_,|             | \t" BLUE "TextEd\n"
			  RED "    |    TextEd   | \t" BLUE "Version: " VERSION "\n"
			  RED "    |  ,------------\n"
			  RED "    \\_/___________/\n" RESET, stdout);
		return 0;
	}
	
	#ifndef ANDROID
		else if(streq(argv[1], "--credits", 10)) {
			return credits();
	#endif
	
	} else {
		Filename = argv[1];
	}
	
	// Try to create file
	permissions = get_user_permissions(Filename);
	if(permissions == ERR_PERM &&
	   (status = createFile(Filename, permissions)))
	{
		return status;
	}

	if(!(permissions & RD_PERM)) {
		fputs(RED "Failed to read the file: Permission denied!\n" RESET, stderr);
		return -1;
	}

	// Load the file in the LineBuffer
	LineBuffer = LbLoadFile(Filename);

	// Initialize command handler
	Command.args = calloc(ARGS_NUM, sizeof(char*));

	fputs(BOLD YELLOW"Welcome in Texted - " RELEASE RESET"\n", stdout);

	// MAIN LOOP
	for(;;)
	{
		printf(BOLD "%s%s > "RESET,
			   get_temp() ? get_user_permission_color(TMP_PATH) : get_user_permission_color(Filename),
			   Filename);
		
		Command.command = getchar();

		// Handle extended ASCII Table
		if(Command.command < 0) {
			PAUSE();
			fprintf(stderr, RED "Invalid command\n" RESET);
			Command.command = '\0';
			continue;
		}

		switch (Command.command)
		{
		case 'p': // PRINT MODE
			// Read arguments
			Command.args[0] = malloc(ARG_SIZE);
			fgets(Command.args[0], ARG_SIZE - 1, stdin);

			// Interpet arguments
			if (streq(Command.args[0], "\n", 1)) {
				ed_print(LineBuffer, 0);
			} else if (streq(Command.args[0], "n\n", 2)) {
				ed_print(LineBuffer, 1);
			} else if (streq(Command.args[0], "l\n", 2)) {
				fputs(getLine(LineBuffer, Line), stdout);

				// Newline coherence
				if(Line != LineBuffer->LB_Size)
					goto exit_print;
			} else if (streq(Command.args[0], "ln\n", 3)) {
				printf("%lu   %s", Line, getLine(LineBuffer, Line));

				// Newline coherence
				if(Line != LineBuffer->LB_Size)
					goto exit_print;
			}
			else if(streq(Command.args[0], " -p\n", 4)) {
				ed_print_permissions(Filename);
				goto exit_print;
			}
			else
			{
				fprintf(stderr, RED "Wrong syntax for the print command\n" RESET);
				if(!(strlen(Command.args[0]) < ARG_SIZE))
					PAUSE();
				goto exit_print;
			}

			if((LineBuffer) && (LineBuffer->LineBuffer))
				putchar('\n');
		
		exit_print:
			free(Command.args[0]);
			break;
		
		case 'i': // INSERT MODE

			// Permission handling
			if(!(permissions & WR_PERM)) {
				fputs(RED "You don't have write permissions on this file!\n" RESET, stderr);
				PAUSE();
				break;
			}

			Command.args[0] = malloc(ARG_SIZE);
			fgets(Command.args[0], ARG_SIZE - 1, stdin);

			if (streq(Command.args[0], "\n", 1)) // Write in RAM
			{
				// Load Buffer
				fputs("--INSERT MODE--\n", stdout);
				Buffer = insert();
				if(!Buffer) {
					goto exit_insert;
				}

				// Make LineBuffer from Buffer
				if(!LineBuffer)
					LineBuffer = getLineBuffer(Buffer);
				else
					LineBuffer = concatenateBuffer(LineBuffer, Buffer);
				
			}
			else if (streq(Command.args[0], "w\n", 2)) // Write directly to file
			{
				// Load Buffer
				fputs("--INSERT MODE--\n", stdout);
				Buffer = insert();
				if(!Buffer) {
					goto exit_insert;
				}

				// Append to file
				app_save(Filename, Buffer);
				printf("Added %lu bytes\n", strlen(Buffer));
				freeLineBuffer(LineBuffer);
				free(Buffer);
				Buffer = NULL;

				// Reload LineBuffer
				LineBuffer = LbLoadFile(Filename);
			}
			else
			{
				Buffer = NULL;
				fputs(RED"Wrong syntax for the insert (i) command\n"RESET, stderr);
			}

		exit_insert:
			PAUSE();
			if(Buffer) {
				free(Buffer);
				Buffer = NULL;
			}
			free(Command.args[0]);
			break;
		
		case 'w': // SAVE
		case 'x': // SAVE AND EXIT

			// Permission handling
			if(!(permissions & WR_PERM)) {
				fputs(RED "You don't have write permissions on this file!\n" RESET, stderr);
				PAUSE();
				break;
			}

			PAUSE();
			Buffer = getBuffer(LineBuffer);

			status = save(Filename, Buffer);
			if(status == ED_NULL_FILE_PTR || status == ED_NULL_PTR) {
				if(errno)
					perror(RED"Failed to write to the file"RESET);
				else
					fputs(RED"Failed to write to the file\n"RESET, stderr);
				free(Buffer);
				Buffer = NULL;
				break;
			} else {
				printf("Written %lu bytes\n", strlen(Buffer));
				free(Buffer);
				Buffer = NULL;
				if (Command.command == 'x')
					goto loop_exit;
			}
			break;
		
		case 's': // SUBSTITUTE WORD
		case 'm': // ADD WORD AFTER

			// Permission handling
			if(!(permissions & WR_PERM)) {
				fputs(RED "You don't have write permissions on this file!\n" RESET, stderr);
				PAUSE();
				break;
			}

			// Handle NULL LineBuffer
			if(!LineBuffer){
				fputs(RED "File is empty!\n" RESET, stderr);
				PAUSE();
				break;
			}
			
			// Read arguments
			status = argumentParser(0, 2, &(Command.args));

			// Error handling
			if(status == ED_ERRNO) {
				perror(RED"Failed to read arguments"RESET);
				Command.command = '\0';
			} else if(status) {
				fprintf(stderr, RED"Wrong syntax for the %s command\n"RESET,
						Command.command == 's' ? "substitute (s)" : "embed (m)");
				Command.command = '\0';
			}

			if (Command.command == 's') {
				if(substitute(getLinePtr(LineBuffer, Line), Command.args[0], Command.args[1]))
					fprintf(stderr, RED "Failed to substitute\n" RESET);
			} else if (Command.command == 'm') {
				if(putstr(getLinePtr(LineBuffer, Line), Command.args[0], Command.args[1]))
					fprintf(stderr, RED "Failed to embed new token\n" RESET);
			}

			break;
		
		case 'a': // ADD WORD AT LINE END

			// Permission handling
			if(!(permissions & WR_PERM)) {
				fputs(RED "You don't have write permissions on this file!\n" RESET, stderr);
				PAUSE();
				break;
			}

			Command.args[1] = NULL;

			// Read and interpret argument
			status = argumentParser(1, 1, &(Command.args));

			// Error Handling
			if(status == ED_ERRNO) {
				perror(RED "Failed to add new word at line end");
				break;
			} else if(status) {
				fprintf(stderr, RED"Wrong syntax for the append (a) command\n"RESET);
				break;
			}

			if(putstr(getLinePtr(LineBuffer, Line), ADD_MODE, Command.args[0]))
				fputs(RED "Failed to append string\n" RESET, stderr);
			break;
		
		case 'l': // SET LINE
			Command.args[0] = malloc(ARG_SIZE);
			fgets(Command.args[0], ARG_SIZE, stdin);

			// "counter" as temporary variable
			counter = strtoul(Command.args[0], NULL, 10);
			if (counter != 0 && counter < ULONG_MAX && counter <= LineBuffer->LB_Size)
				Line = counter;
			else
				fprintf(stderr, RED"Wrong line number\n"RESET);

			if(!strchr(Command.args[0], '\n'))
				PAUSE();
			
			free(Command.args[0]);
			break;
		
		case 'q': // EXIT
			getchar();
			goto loop_exit;
			break;
		
		case 'b': // GET BACKUP
			if(!backup(Filename))
				printf(ITALIC CYAN "Backup file generated: %s\n" RESET, genBackupName(Filename));
			else
				fprintf(stderr, RED "Failed to create a backup of %s: No such file or directory\n", Filename);
			PAUSE();
			break;
		
		case 'h': // PRINT HELP
			//! Control non-sensical arguments
			display_help();
			PAUSE();
			break;
		
		case 'n': // NEW LINE

			// Permission handling
			if(!(permissions & WR_PERM)) {
				fputs(RED "You don't have write permissions on this file!\n" RESET, stderr);
				PAUSE();
				break;
			}

			Command.args[1] = NULL;

			// Read and interpret argument
			status = argumentParser(0, 1, &(Command.args));

			// Error Handling
			if(status == ED_ERRNO) {
				perror(RED "Failed to add new word at line end");
				break;
			} else if(status) {
				fprintf(stderr, RED"Wrong syntax for the new line (n) command\n"RESET);
				break;
			}

			// Add new line
			if((status = addLine(&LineBuffer, Command.args[0], Line)))
				fprintf(stderr, RED"An error occured while trying to add a new line\n"
						ITALIC "Error code: %d\n"RESET, status);
			break;

		case 'd': // DELETE LINE

			// Permission handling
			if(!(permissions & WR_PERM)) {
				fputs(RED "You don't have write permissions on this file!\n" RESET, stderr);
				PAUSE();
				break;
			}

			PAUSE();
			if(delLine(LineBuffer, Line))
				fprintf(stderr, RED"An error occured while trying to remove line no. %lu\n"
						"Error code: %d\n"RESET, Line, status);
			else {
				if(Line != 1) {
					Line--;
					printf(CYAN ITALIC "New working line set to %lu\n" RESET, Line);
				}
				else
					fputs(CYAN ITALIC "Line 2 became was shifted to line 1\n" RESET, stdout);

				//! Embed in delLine()
				if(LineBuffer->LB_Size == 0) {
					free(LineBuffer);
					LineBuffer = NULL;
				}
			}
			break;

		default:
			if(Command.command != '\n')
				PAUSE();
			fprintf(stderr, RED "Invalid command\n" RESET);
			break;
		}
	}

loop_exit:
	remove(TMP_PATH);
	free(Command.args);
	freeLineBuffer(LineBuffer);
	return 0;
}
