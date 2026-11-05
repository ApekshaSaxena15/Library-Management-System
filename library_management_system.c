/*
 * Library Management System
 * ------------------------
 * A beginner-friendly, single-file library application written in standard C.
 *
 * Design notes:
 * - Uses only standard C library headers; no third-party dependencies.
 * - Uses fixed-size arrays, structures, loops, conditions, and functions.
 * - Keeps responsibilities separated into small, descriptive functions.
 * - Uses structure pointers where they make record updates clearer.
 * - Stores data in three text files in the current working directory:
 *      books.txt, members.txt, loans.txt
 *
 * Build:
 *      gcc -std=c11 -Wall -Wextra -pedantic library.c -o library
 *
 * Run:
 *      ./library
 *
 * Data files are created automatically when you choose "Save data" or exit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* --------------------------- Application limits -------------------------- */

#define MAX_BOOKS       500
#define MAX_MEMBERS     200
#define MAX_LOANS       1000

#define TITLE_LENGTH    100
#define AUTHOR_LENGTH   100
#define ISBN_LENGTH     20
#define NAME_LENGTH     100
#define EMAIL_LENGTH    100
#define PHONE_LENGTH    25

#define BOOKS_FILE      "books.txt"
#define MEMBERS_FILE    "members.txt"
#define LOANS_FILE      "loans.txt"

#define LOAN_PERIOD_DAYS 14
#define MAX_BOOKS_PER_MEMBER 5
#define FINE_PER_DAY 5

/* ------------------------------ Data models ------------------------------ */

/*
 * A book is one catalog entry. totalCopies is the number owned by the
 * library; availableCopies is the number that can currently be borrowed.
 */
typedef struct {
    int id;
    char title[TITLE_LENGTH];
    char author[AUTHOR_LENGTH];
    char isbn[ISBN_LENGTH];
    int publicationYear;
    int totalCopies;
    int availableCopies;
    int borrowCount;
} Book;

/* A member is a registered library borrower. */
typedef struct {
    int id;
    char name[NAME_LENGTH];
    char email[EMAIL_LENGTH];
    char phone[PHONE_LENGTH];
    int booksBorrowed;
} Member;

/*
 * A loan connects one member to one book.
 * Dates are stored as day numbers so overdue calculations are straightforward.
 * isReturned is 0 for an active loan and 1 for a completed loan.
 */
typedef struct {
    int id;
    int bookId;
    int memberId;
    int issueDayNumber;
    int dueDayNumber;
    int returnDayNumber;
    int isReturned;
} Loan;

/* --------------------------- Shared application data --------------------- */

static Book books[MAX_BOOKS];
static Member members[MAX_MEMBERS];
static Loan loans[MAX_LOANS];

static int bookCount = 0;
static int memberCount = 0;
static int loanCount = 0;

static int nextBookId = 1;
static int nextMemberId = 1;
static int nextLoanId = 1;

/* -------------------------- Input and display ---------------------------- */

/* Remove the newline left by fgets, if one exists. */
static void removeNewline(char *text) {
    size_t length = strlen(text);
    if (length > 0 && text[length - 1] == '\n') {
        text[length - 1] = '\0';
    }
}

/*
 * Read a complete line safely. If the input is longer than the buffer,
 * discard the remainder so it cannot accidentally become the next answer.
 */
static void readLine(const char *prompt, char *buffer, size_t bufferSize) {
    int character;

    printf("%s", prompt);

    if (fgets(buffer, (int)bufferSize, stdin) == NULL) {
        printf("\nInput ended. Exiting safely.\n");
        exit(EXIT_SUCCESS);
    }

    if (strchr(buffer, '\n') == NULL) {
        while ((character = getchar()) != '\n' && character != EOF) {
            /* Discard extra characters from the same input line. */
        }
    }

    removeNewline(buffer);
}

/* Read a non-empty text value. */
static void readNonEmptyText(const char *prompt, char *buffer, size_t bufferSize) {
    do {
        readLine(prompt, buffer, bufferSize);
        if (buffer[0] == '\0') {
            printf("  Please enter a value; this field cannot be empty.\n");
        }
    } while (buffer[0] == '\0');
}

/* Read an integer and reject non-numeric or out-of-range input. */
static int readInteger(const char *prompt, int minimum, int maximum) {
    char input[100];
    char *endPointer;
    long value;

    for (;;) {
        readLine(prompt, input, sizeof(input));

        value = strtol(input, &endPointer, 10);

        while (isspace((unsigned char)*endPointer)) {
            endPointer++;
        }

        if (input[0] == '\0' || *endPointer != '\0') {
            printf("  Please enter a whole number.\n");
            continue;
        }

        if (value < minimum || value > maximum) {
            printf("  Enter a number between %d and %d.\n", minimum, maximum);
            continue;
        }

        return (int)value;
    }
}

/* Print a consistent section heading for the console interface. */
static void printHeader(const char *title) {
    printf("\n");
    printf("============================================================\n");
    printf("  %s\n", title);
    printf("============================================================\n");
}

/* Pause between screens so users can read the result. */
static void pauseForUser(void) {
    char ignored[8];
    readLine("\nPress ENTER to continue...", ignored, sizeof(ignored));
}

/* ----------------------------- Date helpers ------------------------------ */

/*
 * Convert a Gregorian date to an integer day number.
 * The algorithm is compact and avoids relying on platform-specific time APIs
 * for date arithmetic.
 */
static int dateToDayNumber(int day, int month, int year) {
    int adjustedYear = year - (month <= 2);
    int era = (adjustedYear >= 0 ? adjustedYear : adjustedYear - 399) / 400;
    unsigned yearOfEra = (unsigned)(adjustedYear - era * 400);
    unsigned adjustedMonth = (unsigned)(month + (month > 2 ? -3 : 9));
    unsigned dayOfYear = (153 * adjustedMonth + 2) / 5 + (unsigned)day - 1;
    unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4
                      - yearOfEra / 100 + dayOfYear;

    return era * 146097 + (int)dayOfEra;
}

/* Convert a day number back to a Gregorian date. */
static void dayNumberToDate(int dayNumber, int *day, int *month, int *year) {
    int shifted = dayNumber + 719468;
    int era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
    unsigned dayOfEra = (unsigned)(shifted - era * 146097);
    unsigned yearOfEra = (dayOfEra - dayOfEra / 1460
                        + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    int calculatedYear = (int)yearOfEra + era * 400;
    unsigned dayOfYear = dayOfEra - (365 * yearOfEra
                         + yearOfEra / 4 - yearOfEra / 100);
    unsigned monthPart = (5 * dayOfYear + 2) / 153;
    unsigned calculatedDay = dayOfYear - (153 * monthPart + 2) / 5 + 1;
    unsigned calculatedMonth = monthPart + (monthPart < 10 ? 3 : (unsigned)-9);

    calculatedYear += calculatedMonth <= 2;

    *day = (int)calculatedDay;
    *month = (int)calculatedMonth;
    *year = calculatedYear;
}

/* Get today's date as a day number using the system clock. */
static int todayDayNumber(void) {
    time_t currentTime = time(NULL);
    struct tm *localDate = localtime(&currentTime);

    if (localDate == NULL) {
        return dateToDayNumber(1, 1, 2025);
    }

    return dateToDayNumber(localDate->tm_mday,
                           localDate->tm_mon + 1,
                           localDate->tm_year + 1900);
}

/* Print a day number as DD-MM-YYYY. */
static void printDate(int dayNumber) {
    int day, month, year;
    dayNumberToDate(dayNumber, &day, &month, &year);
    printf("%02d-%02d-%04d", day, month, year);
}

/* --------------------------- Record lookups ----------------------------- */

static int findBookIndexById(int id) {
    int index;
    for (index = 0; index < bookCount; index++) {
        if (books[index].id == id) {
            return index;
        }
    }
    return -1;
}

static int findMemberIndexById(int id) {
    int index;
    for (index = 0; index < memberCount; index++) {
        if (members[index].id == id) {
            return index;
        }
    }
    return -1;
}

static int findLoanIndexById(int id) {
    int index;
    for (index = 0; index < loanCount; index++) {
        if (loans[index].id == id) {
            return index;
        }
    }
    return -1;
}

static int hasActiveLoanForBook(int bookId) {
    int index;
    for (index = 0; index < loanCount; index++) {
        if (loans[index].bookId == bookId && !loans[index].isReturned) {
            return 1;
        }
    }
    return 0;
}

static int hasActiveLoanForMember(int memberId) {
    int index;
    for (index = 0; index < loanCount; index++) {
        if (loans[index].memberId == memberId && !loans[index].isReturned) {
            return 1;
        }
    }
    return 0;
}

static int countActiveLoansForMember(int memberId) {
    int index;
    int count = 0;

    for (index = 0; index < loanCount; index++) {
        if (loans[index].memberId == memberId && !loans[index].isReturned) {
            count++;
        }
    }
    return count;
}

static int containsIgnoreCase(const char *text, const char *pattern) {
    size_t textIndex;

    if (pattern[0] == '\0') {
        return 1;
    }

    for (textIndex = 0; text[textIndex] != '\0'; textIndex++) {
        size_t patternIndex = 0;

        while (text[textIndex + patternIndex] != '\0' &&
               pattern[patternIndex] != '\0' &&
               tolower((unsigned char)text[textIndex + patternIndex]) ==
               tolower((unsigned char)pattern[patternIndex])) {
            patternIndex++;
        }

        if (pattern[patternIndex] == '\0') {
            return 1;
        }
    }

    return 0;
}


/* ----------------------------- Book features ---------------------------- */

static void displayBook(const Book *book) {
    printf("\n  Book ID       : %d\n", book->id);
    printf("  Title         : %s\n", book->title);
    printf("  Author        : %s\n", book->author);
    printf("  ISBN          : %s\n", book->isbn);
    printf("  Published     : %d\n", book->publicationYear);
    printf("  Copies        : %d total, %d available\n",
           book->totalCopies, book->availableCopies);
    printf("  Times borrowed: %d\n", book->borrowCount);
}

static void addBook(void) {
    Book *newBook;

    printHeader("ADD A NEW BOOK");

    if (bookCount >= MAX_BOOKS) {
        printf("The catalog is full (%d books maximum).\n", MAX_BOOKS);
        return;
    }

    newBook = &books[bookCount];
    newBook->id = nextBookId++;

    readNonEmptyText("  Book title: ", newBook->title, sizeof(newBook->title));
    readNonEmptyText("  Author: ", newBook->author, sizeof(newBook->author));
    readNonEmptyText("  ISBN: ", newBook->isbn, sizeof(newBook->isbn));
    newBook->publicationYear = readInteger("  Publication year (1450-2100): ",
                                           1450, 2100);
    newBook->totalCopies = readInteger("  Number of copies: ", 1, 10000);
    newBook->availableCopies = newBook->totalCopies;
    newBook->borrowCount = 0;

    bookCount++;

    printf("\n  Book added successfully! Its library ID is %d.\n", newBook->id);
}

static void displayAllBooks(void) {
    int index;

    printHeader("LIBRARY BOOK CATALOG");

    if (bookCount == 0) {
        printf("The catalog is empty. Add a book to get started.\n");
        return;
    }

    printf("%-6s %-30s %-22s %-8s %-10s\n",
           "ID", "TITLE", "AUTHOR", "YEAR", "AVAILABLE");
    printf("--------------------------------------------------------------------------\n");

    for (index = 0; index < bookCount; index++) {
        printf("%-6d %-30.30s %-22.22s %-8d %d/%d\n",
               books[index].id,
               books[index].title,
               books[index].author,
               books[index].publicationYear,
               books[index].availableCopies,
               books[index].totalCopies);
    }

    printf("\nShowing %d book record(s).\n", bookCount);
}

static void searchBooks(void) {
    int choice;
    int index;
    int matches = 0;
    char searchText[100];

    printHeader("FIND A BOOK");
    printf("  1. Search by title\n");
    printf("  2. Search by author\n");
    printf("  3. Search by ISBN\n");
    printf("  4. Search by book ID\n");
    printf("  0. Back\n");

    choice = readInteger("\n  Choose a search method: ", 0, 4);
    if (choice == 0) {
        return;
    }

    if (choice == 4) {
        int id = readInteger("  Enter book ID: ", 1, 1000000000);
        index = findBookIndexById(id);

        if (index == -1) {
            printf("\n  No book found with ID %d.\n", id);
        } else {
            displayBook(&books[index]);
        }
        return;
    }

    readNonEmptyText("  Enter search text: ", searchText, sizeof(searchText));

    for (index = 0; index < bookCount; index++) {
        int found = 0;

        if (choice == 1 && containsIgnoreCase(books[index].title, searchText)) {
            found = 1;
        } else if (choice == 2 &&
                   containsIgnoreCase(books[index].author, searchText)) {
            found = 1;
        } else if (choice == 3 &&
                   containsIgnoreCase(books[index].isbn, searchText)) {
            found = 1;
        }

        if (found) {
            displayBook(&books[index]);
            matches++;
        }
    }

    printf("\n  Search complete: %d matching book(s).\n", matches);
}

/*
 * Standard C does not define strcasestr. This portable helper searches
 * case-insensitively and is used by the book and member search features.
 */

/* Update the details of an existing book without changing its ID. */
static void updateBook(void) {
    int id;
    int index;
    Book *book;

    printHeader("UPDATE BOOK DETAILS");

    id = readInteger("  Enter the book ID to update: ", 1, 1000000000);
    index = findBookIndexById(id);

    if (index == -1) {
        printf("\n  No book exists with ID %d.\n", id);
        return;
    }

    book = &books[index];
    printf("\n  Current details:\n");
    displayBook(book);
    printf("\n  Press ENTER to keep a field unchanged.\n");

    {
        char input[TITLE_LENGTH];
        readLine("  New title: ", input, sizeof(input));
        if (input[0] != '\0') {
            strcpy(book->title, input);
        }
    }
    {
        char input[AUTHOR_LENGTH];
        readLine("  New author: ", input, sizeof(input));
        if (input[0] != '\0') {
            strcpy(book->author, input);
        }
    }
    {
        char input[ISBN_LENGTH];
        readLine("  New ISBN: ", input, sizeof(input));
        if (input[0] != '\0') {
            strcpy(book->isbn, input);
        }
    }

    book->publicationYear = readInteger("  New publication year (1450-2100): ",
                                        1450, 2100);

    {
        int borrowedCopies = book->totalCopies - book->availableCopies;
        int newTotal = readInteger("  New total number of copies: ",
                                   borrowedCopies, 10000);

        book->totalCopies = newTotal;
        book->availableCopies = newTotal - borrowedCopies;
    }

    printf("\n  Book details updated successfully.\n");
}

/* Remove a book only when no copies are currently on loan. */
static void deleteBook(void) {
    int id;
    int index;
    int moveIndex;

    printHeader("REMOVE A BOOK");

    id = readInteger("  Enter the book ID to remove: ", 1, 1000000000);
    index = findBookIndexById(id);

    if (index == -1) {
        printf("\n  No book exists with ID %d.\n", id);
        return;
    }

    if (hasActiveLoanForBook(id)) {
        printf("\n  This book has active loans and cannot be removed yet.\n");
        return;
    }

    printf("\n  You are about to remove: %s\n", books[index].title);

    {
        char confirmation[10];
        readLine("  Type YES to confirm removal: ", confirmation,
                 sizeof(confirmation));

        if (strcmp(confirmation, "YES") != 0) {
            printf("  Removal cancelled.\n");
            return;
        }
    }

    for (moveIndex = index; moveIndex < bookCount - 1; moveIndex++) {
        books[moveIndex] = books[moveIndex + 1];
    }

    bookCount--;
    printf("\n  Book removed from the catalog.\n");
}

static void sortBooksByTitle(void) {
    int first;
    int second;

    /* Bubble sort is intentionally used because it is easy to understand. */
    for (first = 0; first < bookCount - 1; first++) {
        for (second = 0; second < bookCount - first - 1; second++) {
            if (strcmp(books[second].title, books[second + 1].title) > 0) {
                Book temporary = books[second];
                books[second] = books[second + 1];
                books[second + 1] = temporary;
            }
        }
    }

    printf("\n  Catalog sorted alphabetically by title.\n");
}

static void sortBooksByYear(void) {
    int first;
    int second;

    for (first = 0; first < bookCount - 1; first++) {
        for (second = 0; second < bookCount - first - 1; second++) {
            if (books[second].publicationYear > books[second + 1].publicationYear) {
                Book temporary = books[second];
                books[second] = books[second + 1];
                books[second + 1] = temporary;
            }
        }
    }

    printf("\n  Catalog sorted by publication year (oldest first).\n");
}

/* ---------------------------- Member features --------------------------- */

static void displayMember(const Member *member) {
    printf("\n  Member ID     : %d\n", member->id);
    printf("  Name          : %s\n", member->name);
    printf("  Email         : %s\n", member->email);
    printf("  Phone         : %s\n", member->phone);
    printf("  Active loans  : %d\n", countActiveLoansForMember(member->id));
}

static void registerMember(void) {
    Member *newMember;

    printHeader("REGISTER A MEMBER");

    if (memberCount >= MAX_MEMBERS) {
        printf("The member register is full (%d members maximum).\n", MAX_MEMBERS);
        return;
    }

    newMember = &members[memberCount];
    newMember->id = nextMemberId++;

    readNonEmptyText("  Full name: ", newMember->name, sizeof(newMember->name));
    readNonEmptyText("  Email address: ", newMember->email, sizeof(newMember->email));
    readNonEmptyText("  Phone number: ", newMember->phone, sizeof(newMember->phone));
    newMember->booksBorrowed = 0;

    memberCount++;

    printf("\n  Member registered successfully! Member ID: %d\n", newMember->id);
}

static void displayAllMembers(void) {
    int index;

    printHeader("REGISTERED MEMBERS");

    if (memberCount == 0) {
        printf("No members are registered yet.\n");
        return;
    }

    printf("%-8s %-28s %-30s %-16s %-8s\n",
           "ID", "NAME", "EMAIL", "PHONE", "LOANS");
    printf("--------------------------------------------------------------------------------------\n");

    for (index = 0; index < memberCount; index++) {
        printf("%-8d %-28.28s %-30.30s %-16.16s %-8d\n",
               members[index].id,
               members[index].name,
               members[index].email,
               members[index].phone,
               countActiveLoansForMember(members[index].id));
    }
}

static void searchMembers(void) {
    int choice;
    int index;
    int matches = 0;
    char searchText[100];

    printHeader("FIND A MEMBER");
    printf("  1. Search by name\n");
    printf("  2. Search by member ID\n");
    printf("  0. Back\n");

    choice = readInteger("\n  Choose a search method: ", 0, 2);
    if (choice == 0) {
        return;
    }

    if (choice == 2) {
        int id = readInteger("  Enter member ID: ", 1, 1000000000);
        index = findMemberIndexById(id);

        if (index == -1) {
            printf("\n  No member found with ID %d.\n", id);
        } else {
            displayMember(&members[index]);
        }
        return;
    }

    readNonEmptyText("  Enter member name: ", searchText, sizeof(searchText));

    for (index = 0; index < memberCount; index++) {
        if (containsIgnoreCase(members[index].name, searchText)) {
            displayMember(&members[index]);
            matches++;
        }
    }

    printf("\n  Search complete: %d matching member(s).\n", matches);
}

static void updateMember(void) {
    int id;
    int index;
    Member *member;

    printHeader("UPDATE MEMBER DETAILS");

    id = readInteger("  Enter member ID to update: ", 1, 1000000000);
    index = findMemberIndexById(id);

    if (index == -1) {
        printf("\n  No member exists with ID %d.\n", id);
        return;
    }

    member = &members[index];
    printf("\n  Current details:\n");
    displayMember(member);
    printf("\n  Press ENTER to keep a field unchanged.\n");

    {
        char input[NAME_LENGTH];
        readLine("  New name: ", input, sizeof(input));
        if (input[0] != '\0') {
            strcpy(member->name, input);
        }
    }
    {
        char input[EMAIL_LENGTH];
        readLine("  New email: ", input, sizeof(input));
        if (input[0] != '\0') {
            strcpy(member->email, input);
        }
    }
    {
        char input[PHONE_LENGTH];
        readLine("  New phone: ", input, sizeof(input));
        if (input[0] != '\0') {
            strcpy(member->phone, input);
        }
    }

    printf("\n  Member details updated successfully.\n");
}

static void deleteMember(void) {
    int id;
    int index;
    int moveIndex;

    printHeader("REMOVE A MEMBER");

    id = readInteger("  Enter member ID to remove: ", 1, 1000000000);
    index = findMemberIndexById(id);

    if (index == -1) {
        printf("\n  No member exists with ID %d.\n", id);
        return;
    }

    if (hasActiveLoanForMember(id)) {
        printf("\n  This member has active loans and cannot be removed.\n");
        return;
    }

    printf("\n  You are about to remove member: %s\n", members[index].name);

    {
        char confirmation[10];
        readLine("  Type YES to confirm removal: ", confirmation,
                 sizeof(confirmation));

        if (strcmp(confirmation, "YES") != 0) {
            printf("  Removal cancelled.\n");
            return;
        }
    }

    for (moveIndex = index; moveIndex < memberCount - 1; moveIndex++) {
        members[moveIndex] = members[moveIndex + 1];
    }

    memberCount--;
    printf("\n  Member removed from the register.\n");
}

/* -------------------------- Loan and return features --------------------- */

static void issueBook(void) {
    int bookId;
    int memberId;
    int bookIndex;
    int memberIndex;
    Loan *newLoan;
    int today = todayDayNumber();

    printHeader("ISSUE A BOOK");

    if (loanCount >= MAX_LOANS) {
        printf("The loan history is full (%d records maximum).\n", MAX_LOANS);
        return;
    }

    bookId = readInteger("  Book ID: ", 1, 1000000000);
    bookIndex = findBookIndexById(bookId);

    if (bookIndex == -1) {
        printf("\n  That book ID does not exist. Please check the catalog.\n");
        return;
    }

    if (books[bookIndex].availableCopies <= 0) {
        printf("\n  Sorry, no copies of \"%s\" are currently available.\n",
               books[bookIndex].title);
        return;
    }

    memberId = readInteger("  Member ID: ", 1, 1000000000);
    memberIndex = findMemberIndexById(memberId);

    if (memberIndex == -1) {
        printf("\n  That member ID is not registered.\n");
        return;
    }

    if (countActiveLoansForMember(memberId) >= MAX_BOOKS_PER_MEMBER) {
        printf("\n  This member has reached the borrowing limit (%d books).\n",
               MAX_BOOKS_PER_MEMBER);
        return;
    }

    newLoan = &loans[loanCount];
    newLoan->id = nextLoanId++;
    newLoan->bookId = bookId;
    newLoan->memberId = memberId;
    newLoan->issueDayNumber = today;
    newLoan->dueDayNumber = today + LOAN_PERIOD_DAYS;
    newLoan->returnDayNumber = 0;
    newLoan->isReturned = 0;

    loanCount++;
    books[bookIndex].availableCopies--;
    books[bookIndex].borrowCount++;

    printf("\n  BOOK ISSUED SUCCESSFULLY\n");
    printf("  Book   : %s\n", books[bookIndex].title);
    printf("  Member : %s\n", members[memberIndex].name);
    printf("  Due    : ");
    printDate(newLoan->dueDayNumber);
    printf("\n  Please return the book by the due date to avoid a fine.\n");
}

static void returnBook(void) {
    int loanId;
    int loanIndex;
    int bookIndex;
    int memberIndex;
    int today = todayDayNumber();
    int overdueDays;
    int fine;

    printHeader("RETURN A BOOK");

    loanId = readInteger("  Loan ID (shown in the issued-books list): ",
                         1, 1000000000);
    loanIndex = findLoanIndexById(loanId);

    if (loanIndex == -1 || loans[loanIndex].isReturned) {
        printf("\n  No active loan exists with that ID.\n");
        return;
    }

    bookIndex = findBookIndexById(loans[loanIndex].bookId);
    memberIndex = findMemberIndexById(loans[loanIndex].memberId);

    if (bookIndex == -1 || memberIndex == -1) {
        printf("\n  The loan record is inconsistent. Please contact the librarian.\n");
        return;
    }

    overdueDays = today - loans[loanIndex].dueDayNumber;
    if (overdueDays < 0) {
        overdueDays = 0;
    }
    fine = overdueDays * FINE_PER_DAY;

    loans[loanIndex].isReturned = 1;
    loans[loanIndex].returnDayNumber = today;
    books[bookIndex].availableCopies++;

    printf("\n  RETURN COMPLETED\n");
    printf("  Book   : %s\n", books[bookIndex].title);
    printf("  Member : %s\n", members[memberIndex].name);

    if (overdueDays > 0) {
        printf("  Overdue: %d day(s)\n", overdueDays);
        printf("  Fine   : %d currency unit(s) (%d per day)\n",
               fine, FINE_PER_DAY);
    } else {
        printf("  Status : Returned on time. No fine.\n");
    }
}

static void displayIssuedBooks(void) {
    int index;
    int activeCount = 0;

    printHeader("CURRENTLY ISSUED BOOKS");

    printf("%-8s %-8s %-8s %-24s %-24s %-12s\n",
           "LOAN ID", "BOOK ID", "MEMBER", "BOOK TITLE", "BORROWER", "DUE DATE");
    printf("--------------------------------------------------------------------------------------\n");

    for (index = 0; index < loanCount; index++) {
        if (!loans[index].isReturned) {
            int bookIndex = findBookIndexById(loans[index].bookId);
            int memberIndex = findMemberIndexById(loans[index].memberId);

            if (bookIndex != -1 && memberIndex != -1) {
                printf("%-8d %-8d %-8d %-24.24s %-24.24s ",
                       loans[index].id,
                       loans[index].bookId,
                       loans[index].memberId,
                       books[bookIndex].title,
                       members[memberIndex].name);
                printDate(loans[index].dueDayNumber);
                printf("\n");
                activeCount++;
            }
        }
    }

    if (activeCount == 0) {
        printf("There are no books currently issued.\n");
    } else {
        printf("\n%d active loan(s) listed.\n", activeCount);
    }
}

static void displayMemberLoans(void) {
    int memberId;
    int memberIndex;
    int index;
    int found = 0;

    printHeader("MEMBER'S BORROWED BOOKS");

    memberId = readInteger("  Enter member ID: ", 1, 1000000000);
    memberIndex = findMemberIndexById(memberId);

    if (memberIndex == -1) {
        printf("\n  No member exists with ID %d.\n", memberId);
        return;
    }

    printf("\n  Borrower: %s\n", members[memberIndex].name);

    for (index = 0; index < loanCount; index++) {
        if (loans[index].memberId == memberId && !loans[index].isReturned) {
            int bookIndex = findBookIndexById(loans[index].bookId);

            if (bookIndex != -1) {
                printf("  - %s (Loan ID %d, due ",
                       books[bookIndex].title, loans[index].id);
                printDate(loans[index].dueDayNumber);
                printf(")\n");
                found++;
            }
        }
    }

    if (found == 0) {
        printf("  This member has no active loans.\n");
    }
}

/* ----------------------------- Reports ---------------------------------- */

static void displayUnavailableBooks(void) {
    int index;
    int found = 0;

    printHeader("BOOKS CURRENTLY UNAVAILABLE");

    for (index = 0; index < bookCount; index++) {
        if (books[index].availableCopies == 0) {
            printf("  ID %d | %s by %s\n",
                   books[index].id, books[index].title, books[index].author);
            found++;
        }
    }

    if (found == 0) {
        printf("All catalog books have at least one available copy.\n");
    } else {
        printf("\n%d title(s) currently have no available copies.\n", found);
    }
}

static void displayLibraryStatistics(void) {
    int index;
    int totalCopies = 0;
    int availableCopies = 0;
    int activeLoans = 0;
    int returnedLoans = 0;

    printHeader("LIBRARY OVERVIEW");

    for (index = 0; index < bookCount; index++) {
        totalCopies += books[index].totalCopies;
        availableCopies += books[index].availableCopies;
    }

    for (index = 0; index < loanCount; index++) {
        if (loans[index].isReturned) {
            returnedLoans++;
        } else {
            activeLoans++;
        }
    }

    printf("  Unique book titles : %d\n", bookCount);
    printf("  Total book copies  : %d\n", totalCopies);
    printf("  Available copies   : %d\n", availableCopies);
    printf("  Registered members : %d\n", memberCount);
    printf("  Active loans       : %d\n", activeLoans);
    printf("  Completed loans    : %d\n", returnedLoans);
    printf("  Total loan records : %d\n", loanCount);
}

static void displayMostBorrowedBooks(void) {
    int index;
    int maximum = 0;

    printHeader("MOST BORROWED BOOKS");

    if (bookCount == 0) {
        printf("There are no books in the catalog yet.\n");
        return;
    }

    for (index = 0; index < bookCount; index++) {
        if (books[index].borrowCount > maximum) {
            maximum = books[index].borrowCount;
        }
    }

    if (maximum == 0) {
        printf("No books have been borrowed yet.\n");
        return;
    }

    printf("  Most borrowed title(s):\n");
    for (index = 0; index < bookCount; index++) {
        if (books[index].borrowCount == maximum) {
            printf("  - %s by %s (%d borrow(s))\n",
                   books[index].title, books[index].author, maximum);
        }
    }
}

static void displayMemberStatistics(void) {
    int index;
    int maximum = 0;

    printHeader("MEMBER BORROWING STATISTICS");

    if (memberCount == 0) {
        printf("No members are registered yet.\n");
        return;
    }

    printf("%-8s %-30s %-16s %-16s\n",
           "ID", "MEMBER", "ACTIVE LOANS", "TOTAL BORROWS");
    printf("--------------------------------------------------------------------\n");

    for (index = 0; index < memberCount; index++) {
        int loanIndex;
        int totalBorrows = 0;

        for (loanIndex = 0; loanIndex < loanCount; loanIndex++) {
            if (loans[loanIndex].memberId == members[index].id) {
                totalBorrows++;
            }
        }

        printf("%-8d %-30.30s %-16d %-16d\n",
               members[index].id,
               members[index].name,
               countActiveLoansForMember(members[index].id),
               totalBorrows);

        if (totalBorrows > maximum) {
            maximum = totalBorrows;
        }
    }

    printf("\nHighest lifetime borrowing count: %d\n", maximum);
}

/* --------------------------- File persistence --------------------------- */

/*
 * The files use simple pipe-separated text records.
 * User-entered text containing '|' is replaced with '/' before saving.
 * This keeps the format readable without introducing a complex parser.
 */
static void replacePipeCharacters(char *text) {
    int index;
    for (index = 0; text[index] != '\0'; index++) {
        if (text[index] == '|') {
            text[index] = '/';
        }
    }
}

static int saveBooks(void) {
    FILE *file = fopen(BOOKS_FILE, "w");
    int index;

    if (file == NULL) {
        printf("  Could not open %s for writing.\n", BOOKS_FILE);
        return 0;
    }

    for (index = 0; index < bookCount; index++) {
        replacePipeCharacters(books[index].title);
        replacePipeCharacters(books[index].author);
        replacePipeCharacters(books[index].isbn);

        fprintf(file, "%d|%s|%s|%s|%d|%d|%d|%d\n",
                books[index].id,
                books[index].title,
                books[index].author,
                books[index].isbn,
                books[index].publicationYear,
                books[index].totalCopies,
                books[index].availableCopies,
                books[index].borrowCount);
    }

    fclose(file);
    return 1;
}

static int saveMembers(void) {
    FILE *file = fopen(MEMBERS_FILE, "w");
    int index;

    if (file == NULL) {
        printf("  Could not open %s for writing.\n", MEMBERS_FILE);
        return 0;
    }

    for (index = 0; index < memberCount; index++) {
        replacePipeCharacters(members[index].name);
        replacePipeCharacters(members[index].email);
        replacePipeCharacters(members[index].phone);

        fprintf(file, "%d|%s|%s|%s|%d\n",
                members[index].id,
                members[index].name,
                members[index].email,
                members[index].phone,
                members[index].booksBorrowed);
    }

    fclose(file);
    return 1;
}

static int saveLoans(void) {
    FILE *file = fopen(LOANS_FILE, "w");
    int index;

    if (file == NULL) {
        printf("  Could not open %s for writing.\n", LOANS_FILE);
        return 0;
    }

    for (index = 0; index < loanCount; index++) {
        fprintf(file, "%d|%d|%d|%d|%d|%d|%d\n",
                loans[index].id,
                loans[index].bookId,
                loans[index].memberId,
                loans[index].issueDayNumber,
                loans[index].dueDayNumber,
                loans[index].returnDayNumber,
                loans[index].isReturned);
    }

    fclose(file);
    return 1;
}

static void saveAllData(void) {
    int booksSaved;
    int membersSaved;
    int loansSaved;

    printHeader("SAVE LIBRARY DATA");

    booksSaved = saveBooks();
    membersSaved = saveMembers();
    loansSaved = saveLoans();

    if (booksSaved && membersSaved && loansSaved) {
        printf("  All records saved successfully.\n");
        printf("  Files: %s, %s, %s\n",
               BOOKS_FILE, MEMBERS_FILE, LOANS_FILE);
    } else {
        printf("  Some data could not be saved. Check file permissions and disk space.\n");
    }
}

/* Split a pipe-separated line into fields. Returns the number of fields. */
static int splitFields(char *line, char *fields[], int maximumFields) {
    int count = 0;
    char *token = strtok(line, "|");

    while (token != NULL && count < maximumFields) {
        fields[count++] = token;
        token = strtok(NULL, "|");
    }

    return count;
}

static void loadBooks(void) {
    FILE *file = fopen(BOOKS_FILE, "r");
    char line[600];

    if (file == NULL) {
        return; /* First run: no file exists yet. */
    }

    while (fgets(line, sizeof(line), file) != NULL && bookCount < MAX_BOOKS) {
        char *fields[8];
        int count;
        Book loadedBook;

        removeNewline(line);
        count = splitFields(line, fields, 8);

        if (count != 8) {
            continue;
        }

        loadedBook.id = atoi(fields[0]);
        snprintf(loadedBook.title, sizeof(loadedBook.title), "%s", fields[1]);
        snprintf(loadedBook.author, sizeof(loadedBook.author), "%s", fields[2]);
        snprintf(loadedBook.isbn, sizeof(loadedBook.isbn), "%s", fields[3]);
        loadedBook.publicationYear = atoi(fields[4]);
        loadedBook.totalCopies = atoi(fields[5]);
        loadedBook.availableCopies = atoi(fields[6]);
        loadedBook.borrowCount = atoi(fields[7]);

        if (loadedBook.id <= 0 || loadedBook.totalCopies < 0 ||
            loadedBook.availableCopies < 0 ||
            loadedBook.availableCopies > loadedBook.totalCopies) {
            continue;
        }

        books[bookCount++] = loadedBook;
        if (loadedBook.id >= nextBookId) {
            nextBookId = loadedBook.id + 1;
        }
    }

    fclose(file);
}

static void loadMembers(void) {
    FILE *file = fopen(MEMBERS_FILE, "r");
    char line[400];

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL && memberCount < MAX_MEMBERS) {
        char *fields[5];
        int count;
        Member loadedMember;

        removeNewline(line);
        count = splitFields(line, fields, 5);

        if (count != 5) {
            continue;
        }

        loadedMember.id = atoi(fields[0]);
        snprintf(loadedMember.name, sizeof(loadedMember.name), "%s", fields[1]);
        snprintf(loadedMember.email, sizeof(loadedMember.email), "%s", fields[2]);
        snprintf(loadedMember.phone, sizeof(loadedMember.phone), "%s", fields[3]);
        loadedMember.booksBorrowed = atoi(fields[4]);

        if (loadedMember.id <= 0) {
            continue;
        }

        members[memberCount++] = loadedMember;
        if (loadedMember.id >= nextMemberId) {
            nextMemberId = loadedMember.id + 1;
        }
    }

    fclose(file);
}

static void loadLoans(void) {
    FILE *file = fopen(LOANS_FILE, "r");
    char line[300];

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL && loanCount < MAX_LOANS) {
        char *fields[7];
        int count;
        Loan loadedLoan;

        removeNewline(line);
        count = splitFields(line, fields, 7);

        if (count != 7) {
            continue;
        }

        loadedLoan.id = atoi(fields[0]);
        loadedLoan.bookId = atoi(fields[1]);
        loadedLoan.memberId = atoi(fields[2]);
        loadedLoan.issueDayNumber = atoi(fields[3]);
        loadedLoan.dueDayNumber = atoi(fields[4]);
        loadedLoan.returnDayNumber = atoi(fields[5]);
        loadedLoan.isReturned = atoi(fields[6]);

        if (loadedLoan.id <= 0 ||
            findBookIndexById(loadedLoan.bookId) == -1 ||
            findMemberIndexById(loadedLoan.memberId) == -1) {
            continue;
        }

        loans[loanCount++] = loadedLoan;
        if (loadedLoan.id >= nextLoanId) {
            nextLoanId = loadedLoan.id + 1;
        }
    }

    fclose(file);
}

static void loadAllData(void) {
    loadBooks();
    loadMembers();
    loadLoans();

    /*
     * Recalculate availability from active loans. This protects the inventory
     * count if a saved file was edited or interrupted during a previous run.
     */
    {
        int index;

        for (index = 0; index < bookCount; index++) {
            int loanIndex;
            int activeLoans = 0;

            for (loanIndex = 0; loanIndex < loanCount; loanIndex++) {
                if (loans[loanIndex].bookId == books[index].id &&
                    !loans[loanIndex].isReturned) {
                    activeLoans++;
                }
            }

            if (activeLoans <= books[index].totalCopies) {
                books[index].availableCopies =
                    books[index].totalCopies - activeLoans;
            }
        }
    }
}

/* ------------------------------ Menus ----------------------------------- */

static void bookManagementMenu(void) {
    int choice;

    do {
        printHeader("BOOK MANAGEMENT");
        printf("  1. Add a book\n");
        printf("  2. Display all books\n");
        printf("  3. Search for a book\n");
        printf("  4. Update book details\n");
        printf("  5. Remove a book\n");
        printf("  6. Sort books by title\n");
        printf("  7. Sort books by publication year\n");
        printf("  0. Return to main menu\n");

        choice = readInteger("\n  Your choice: ", 0, 7);

        switch (choice) {
            case 1: addBook(); break;
            case 2: displayAllBooks(); break;
            case 3: searchBooks(); break;
            case 4: updateBook(); break;
            case 5: deleteBook(); break;
            case 6: sortBooksByTitle(); break;
            case 7: sortBooksByYear(); break;
            case 0: break;
        }

        if (choice != 0) {
            pauseForUser();
        }
    } while (choice != 0);
}

static void memberManagementMenu(void) {
    int choice;

    do {
        printHeader("MEMBER MANAGEMENT");
        printf("  1. Register a member\n");
        printf("  2. Display all members\n");
        printf("  3. Search for a member\n");
        printf("  4. Update member details\n");
        printf("  5. Remove a member\n");
        printf("  0. Return to main menu\n");

        choice = readInteger("\n  Your choice: ", 0, 5);

        switch (choice) {
            case 1: registerMember(); break;
            case 2: displayAllMembers(); break;
            case 3: searchMembers(); break;
            case 4: updateMember(); break;
            case 5: deleteMember(); break;
            case 0: break;
        }

        if (choice != 0) {
            pauseForUser();
        }
    } while (choice != 0);
}

static void circulationMenu(void) {
    int choice;

    do {
        printHeader("BOOK CIRCULATION");
        printf("  1. Issue a book\n");
        printf("  2. Return a book\n");
        printf("  3. View currently issued books\n");
        printf("  4. View a member's borrowed books\n");
        printf("  0. Return to main menu\n");

        choice = readInteger("\n  Your choice: ", 0, 4);

        switch (choice) {
            case 1: issueBook(); break;
            case 2: returnBook(); break;
            case 3: displayIssuedBooks(); break;
            case 4: displayMemberLoans(); break;
            case 0: break;
        }

        if (choice != 0) {
            pauseForUser();
        }
    } while (choice != 0);
}

static void reportsMenu(void) {
    int choice;

    do {
        printHeader("SEARCH, SORTING & REPORTS");
        printf("  1. Show unavailable books\n");
        printf("  2. Show most borrowed books\n");
        printf("  3. Show library statistics\n");
        printf("  4. Show member borrowing statistics\n");
        printf("  0. Return to main menu\n");

        choice = readInteger("\n  Your choice: ", 0, 4);

        switch (choice) {
            case 1: displayUnavailableBooks(); break;
            case 2: displayMostBorrowedBooks(); break;
            case 3: displayLibraryStatistics(); break;
            case 4: displayMemberStatistics(); break;
            case 0: break;
        }

        if (choice != 0) {
            pauseForUser();
        }
    } while (choice != 0);
}

static void printWelcome(void) {
    printf("\n");
    printf("  +------------------------------------------------------+\n");
    printf("  |              COMMUNITY LIBRARY SYSTEM               |\n");
    printf("  |        Your books, organized and accessible         |\n");
    printf("  +------------------------------------------------------+\n");
    printf("  Welcome, Librarian! Let's make today's library work easy.\n");
}

/* ------------------------------- Main ----------------------------------- */

int main(void) {
    int choice;

    loadAllData();
    printWelcome();

    do {
        printHeader("MAIN MENU");
        printf("  Books in catalog : %d\n", bookCount);
        printf("  Registered users : %d\n", memberCount);
        printf("  Active loans     : %d\n\n", loanCount);

        printf("  1. Book management\n");
        printf("  2. Member management\n");
        printf("  3. Issue / return books\n");
        printf("  4. Search and reports\n");
        printf("  5. Save data now\n");
        printf("  0. Save and exit\n");

        choice = readInteger("\n  What would you like to do? ", 0, 5);

        switch (choice) {
            case 1:
                bookManagementMenu();
                break;
            case 2:
                memberManagementMenu();
                break;
            case 3:
                circulationMenu();
                break;
            case 4:
                reportsMenu();
                break;
            case 5:
                saveAllData();
                pauseForUser();
                break;
            case 0:
                saveAllData();
                printf("\n  Thank you for keeping the library organized.\n");
                printf("  Goodbye, and have a wonderful day!\n\n");
                break;
        }
    } while (choice != 0);

    return 0;
}
