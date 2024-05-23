#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <vector>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <fstream>
#include <type_traits>
#include <codecvt>
#include <locale>
#include <iostream>
#include <iomanip>
#include <string>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <locale>

using namespace std;

template<typename T>
class Logger {
public:
    static void log(const T& message, const std::wstring& type = L"info") {
        static_assert(std::is_same<T, std::wstring>::value || std::is_same<T, std::string>::value,
            "Logger only supports std::string and std::wstring");

        wofstream logFile("log.txt", ios::app);
        logFile.imbue(std::locale(logFile.getloc(), new std::codecvt_utf8<wchar_t>)); // Set UTF-8 encoding
        logFile << type << L": " << convertToWString(message) << endl;
        logFile.close();
    }

private:
    static std::wstring convertToWString(const std::string& str) {
        std::wstring wstr(str.begin(), str.end());
        return wstr;
    }

    static const std::wstring& convertToWString(const std::wstring& wstr) {
        return wstr;
    }
};

class DatabaseException : public exception {
private:
    string message;
public:
    DatabaseException(const string& msg) : message(msg) {}
    const char* what() const noexcept override {
        return message.c_str();
    }
};

class DatabaseManager {
private:
    SQLHENV hEnv;
    SQLHDBC hDbc;
    SQLHSTMT hStmt;

public:
    DatabaseManager() : hEnv(nullptr), hDbc(nullptr), hStmt(nullptr) {}
    ~DatabaseManager() {
        if (hStmt) SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        if (hDbc) {
            SQLDisconnect(hDbc);
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        }
        if (hEnv) SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    }

    bool connect() {
        SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
        if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) return false;

        SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
        retcode = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
        if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) return false;

        SQLWCHAR connectionString[] = L"DRIVER={ODBC Driver 17 for SQL Server};SERVER=ARZYSHKA;DATABASE=EmployeeTraining;UID=sa;PWD=12345678910";
        retcode = SQLDriverConnectW(hDbc, NULL, connectionString, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
        if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) return false;

        retcode = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        return retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO;
    }

    bool executeQuery(const wstring& query) {
        if (!connect()) {
            Logger<string>::log("Failed to connect to database.", L"error");
            return false;
        }
        SQLRETURN retcode = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
        if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
            SQLWCHAR sqlState[1024], message[1024];
            SQLINTEGER nativeError;
            SQLSMALLINT msgLength;
            SQLGetDiagRec(SQL_HANDLE_STMT, hStmt, 1, sqlState, &nativeError, message, 1024, &msgLength);
            wcerr << L"SQL Error: " << sqlState << L" - " << message << endl;
            throw DatabaseException("Failed to execute query");
        }
        return true;
    }

    SQLHSTMT getStmtHandle() const { return hStmt; }

    bool authenticateUser(const wstring& username, const wstring& password, wstring& userType) {
        wstring query = L"SELECT userType FROM Users WHERE username = '" + username + L"' AND password = '" + password + L"'";
        if (!executeQuery(query)) {
            return false;
        }

        SQLHSTMT stmt = getStmtHandle();
        SQLWCHAR dbUserType[50];

        if (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_WCHAR, dbUserType, sizeof(dbUserType), nullptr);
            userType = dbUserType;
            return true;
        }

        return false;
    }
};

class IManageable {
public:
    virtual void performTasks(DatabaseManager& dbManager) = 0;
    virtual ~IManageable() {}
};

class User : public IManageable {
protected:
    wstring username;
    wstring password;
    wstring userType;
    bool authenticated;

public:
    User(const wstring& username, const wstring& password, const wstring& userType = L"") : username(username), password(password), userType(userType), authenticated(false) {}

    bool authenticate(DatabaseManager& dbManager, const wstring& enteredUsername, const wstring& enteredPassword) {
        wstring dbUserType;
        if (dbManager.authenticateUser(enteredUsername, enteredPassword, dbUserType)) {
            authenticated = true;
            userType = dbUserType;
            return true;
        }
        authenticated = false;
        return false;
    }

    bool isAuthenticated() const { return authenticated; }
    virtual ~User() {}
};

class MaterialManager {
public:
    void addMaterial(DatabaseManager& dbManager, const wstring& topic, const wstring& content) {
        wstring query = L"INSERT INTO TrainingMaterials (title, content) VALUES ('" + topic + L"', '" + content + L"')";
        if (!dbManager.executeQuery(query)) {
            wcout << L"Failed to add material." << endl;
        }
        else {
            wcout << L"Material added successfully." << endl;
        }
    }

    void editMaterial(DatabaseManager& dbManager, int id, const wstring& newTopic, const wstring& newContent) {
        wstring query = L"UPDATE TrainingMaterials SET title = '" + newTopic + L"', content = '" + newContent + L"' WHERE id = " + to_wstring(id);
        if (!dbManager.executeQuery(query)) {
            wcout << L"Failed to update material." << endl;
        }
        else {
            wcout << L"Material updated successfully." << endl;
        }
    }

    void deleteMaterial(DatabaseManager& dbManager, int id) {
        wstring query = L"DELETE FROM TrainingMaterials WHERE id = " + to_wstring(id);
        if (!dbManager.executeQuery(query)) {
            wcout << L"Failed to delete material." << endl;
        }
        else {
            wcout << L"Material deleted successfully." << endl;
        }
    }

    void viewAllMaterials(DatabaseManager& dbManager) {
        wstring query = L"SELECT * FROM TrainingMaterials";
        if (!dbManager.executeQuery(query)) {
            wcerr << L"Failed to execute SQL query." << endl;
            return;
        }

        SQLHSTMT stmt = dbManager.getStmtHandle();
        SQLINTEGER id;
        SQLWCHAR title[255], content[1024];

        wcout << L"Material ID\tTitle\tContent" << endl;
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, nullptr);
            SQLGetData(stmt, 2, SQL_C_WCHAR, title, 255, nullptr);
            SQLGetData(stmt, 3, SQL_C_WCHAR, content, 1024, nullptr);
            wcout << id << L"\t" << title << L"\t" << content << endl;
        }
    }
};

class QuestionManager {
public:
    struct Question {
        int id;
        wstring text;
        wstring option1;
        wstring option2;
        wstring option3;
        int correctAnswer;
    };

    vector<Question> fetchAllQuestions(DatabaseManager& dbManager) {
        wstring query = L"SELECT id, questionText, Option1, Option2, Option3, CorrectAnswer FROM Questions";
        if (!dbManager.executeQuery(query)) {
            wcerr << L"Failed to fetch questions." << endl;
            return {};
        }

        SQLHSTMT stmt = dbManager.getStmtHandle();
        SQLINTEGER id, correctAnswer;
        SQLWCHAR text[1024], option1[255], option2[255], option3[255];
        vector<Question> questions;

        while (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, nullptr);
            SQLGetData(stmt, 2, SQL_C_WCHAR, text, 1024, nullptr);
            SQLGetData(stmt, 3, SQL_C_WCHAR, option1, 255, nullptr);
            SQLGetData(stmt, 4, SQL_C_WCHAR, option2, 255, nullptr);
            SQLGetData(stmt, 5, SQL_C_WCHAR, option3, 255, nullptr);
            SQLGetData(stmt, 6, SQL_C_SLONG, &correctAnswer, 0, nullptr);

            questions.push_back({ id, text, option1, option2, option3, correctAnswer });
        }

        return questions;
    }

    void addQuestion(DatabaseManager& dbManager, const wstring& text, const wstring& option1, const wstring& option2, const wstring& option3, int correctOption) {
        wstring query = L"INSERT INTO Questions (questionText, Option1, Option2, Option3, CorrectAnswer) VALUES ('"
            + text + L"', '" + option1 + L"', '" + option2 + L"', '" + option3 + L"', " + to_wstring(correctOption) + L")";
        if (!dbManager.executeQuery(query)) {
            wcout << L"Failed to add question." << endl;
        }
        else {
            wcout << L"Question added successfully." << endl;
        }
    }

    void editQuestion(DatabaseManager& dbManager, int id, const wstring& newText, const wstring& newOption1, const wstring& newOption2, const wstring& newOption3, int newCorrectOption) {
        wstring query = L"UPDATE Questions SET questionText = '" + newText + L"', Option1 = '" + newOption1 + L"', Option2 = '"
            + newOption2 + L"', Option3 = '" + newOption3 + L"', CorrectAnswer = " + to_wstring(newCorrectOption) + L" WHERE id = " + to_wstring(id);
        if (!dbManager.executeQuery(query)) {
            wcout << L"Failed to update question." << endl;
        }
        else {
            wcout << L"Question updated successfully." << endl;
        }
    }

    void deleteQuestion(DatabaseManager& dbManager, int id) {
        wstring query = L"DELETE FROM Questions WHERE id = " + to_wstring(id);
        if (!dbManager.executeQuery(query)) {
            wcout << L"Failed to delete question." << endl;
        }
        else {
            wcout << L"Question deleted successfully." << endl;
        }
    }

    void viewAllQuestions(DatabaseManager& dbManager) {
        wstring query = L"SELECT * FROM Questions";
        if (!dbManager.executeQuery(query)) {
            wcerr << L"Failed to execute SQL query." << endl;
            return;
        }

        SQLHSTMT stmt = dbManager.getStmtHandle();
        SQLINTEGER id, correctAnswer;
        SQLWCHAR text[1024], option1[255], option2[255], option3[255];

        wcout << left << setw(12) << L"Question ID"
            << left << setw(30) << L"Text"
            << left << setw(20) << L"Option 1"
            << left << setw(20) << L"Option 2"
            << left << setw(20) << L"Option 3"
            << left << setw(15) << L"Correct Answer" << endl;

        wcout << left << setw(12) << L"-----------"
            << left << setw(30) << L"----"
            << left << setw(20) << L"--------"
            << left << setw(20) << L"--------"
            << left << setw(20) << L"--------"
            << left << setw(15) << L"--------------" << endl;

        while (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, nullptr);
            SQLGetData(stmt, 2, SQL_C_WCHAR, text, 1024, nullptr);
            SQLGetData(stmt, 3, SQL_C_WCHAR, option1, 255, nullptr);
            SQLGetData(stmt, 4, SQL_C_WCHAR, option2, 255, nullptr);
            SQLGetData(stmt, 5, SQL_C_WCHAR, option3, 255, nullptr);
            SQLGetData(stmt, 6, SQL_C_SLONG, &correctAnswer, 0, nullptr);

            wcout << left << setw(12) << id
                << left << setw(30) << text
                << left << setw(20) << option1
                << left << setw(20) << option2
                << left << setw(20) << option3
                << left << setw(15) << correctAnswer << endl;
        }
    }
};

class TestResultManager {
public:
    void saveTestResults(DatabaseManager& dbManager, const wstring& username, int testId, int score) {
        wstring query = L"INSERT INTO TestResults (employeeUsername, testId, score, testDate) VALUES ('" + username + L"', " + to_wstring(testId) + L", " + to_wstring(score) + L", GETDATE())";
        if (!dbManager.executeQuery(query)) {
            wcout << L"Failed to save test results." << endl;
        }
        else {
            wcout << L"Test results saved successfully." << endl;
        }
    }

    void viewTestResults(DatabaseManager& dbManager) {
        wstring query = L"SELECT employeeUsername, testId, score, testDate FROM TestResults ORDER BY testDate DESC";
        if (!dbManager.executeQuery(query)) {
            wcerr << L"Failed to fetch test results." << endl;
            return;
        }

        SQLHSTMT stmt = dbManager.getStmtHandle();
        SQLWCHAR username[255];
        SQLINTEGER testId, score;
        SQL_TIMESTAMP_STRUCT testDate;

        wcout << L"Username\tTest ID\tScore\tTest Date" << endl;
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_WCHAR, &username, sizeof(username), NULL);
            SQLGetData(stmt, 2, SQL_C_SLONG, &testId, 0, NULL);
            SQLGetData(stmt, 3, SQL_C_SLONG, &score, 0, NULL);
            SQLGetData(stmt, 4, SQL_C_TYPE_TIMESTAMP, &testDate, 0, NULL);

            wchar_t dateStr[100];
            swprintf(dateStr, 100, L"%04d-%02d-%02d %02d:%02d:%02d",
                testDate.year, testDate.month, testDate.day,
                testDate.hour, testDate.minute, testDate.second);

            wcout << username << L"\t" << testId << L"\t" << score << L"\t" << dateStr << endl;
        }
    }

    void viewEmployeeTestResults(DatabaseManager& dbManager, const wstring& username) {
        wstring query = L"SELECT testId, score, testDate FROM TestResults WHERE employeeUsername = '" + username + L"' ORDER BY testDate DESC";
        if (!dbManager.executeQuery(query)) {
            wcerr << L"Failed to fetch test results for employee." << endl;
            return;
        }

        SQLHSTMT stmt = dbManager.getStmtHandle();
        SQLINTEGER testId, score;
        SQL_TIMESTAMP_STRUCT testDate;

        wcout << L"Test ID\tScore\tTest Date" << endl;
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_SLONG, &testId, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_SLONG, &score, 0, NULL);
            SQLGetData(stmt, 3, SQL_C_TYPE_TIMESTAMP, &testDate, 0, NULL);

            wchar_t dateStr[100];
            swprintf(dateStr, 100, L"%04d-%02d-%02d %02d:%02d-%02d",
                testDate.year, testDate.month, testDate.day,
                testDate.hour, testDate.minute, testDate.second);

            wcout << testId << L"\t" << score << L"\t" << dateStr << endl;
        }
    }

    void printTestResultsToFile(DatabaseManager& dbManager) {
        wstring query = L"SELECT employeeUsername, testId, score, testDate FROM TestResults ORDER BY testDate DESC";
        if (!dbManager.executeQuery(query)) {
            wcerr << L"Failed to fetch test results." << endl;
            return;
        }

        wofstream outfile("test_results.txt");
        outfile.imbue(locale(outfile.getloc(), new codecvt_utf8<wchar_t>)); // Set UTF-8 encoding for output file
        outfile << L"Username\tTest ID\tScore\tTest Date\n";

        SQLHSTMT stmt = dbManager.getStmtHandle();
        SQLWCHAR username[255];
        SQLINTEGER testId, score;
        SQL_TIMESTAMP_STRUCT testDate;

        while (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_WCHAR, &username, sizeof(username), NULL);
            SQLGetData(stmt, 2, SQL_C_SLONG, &testId, 0, NULL);
            SQLGetData(stmt, 3, SQL_C_SLONG, &score, 0, NULL);
            SQLGetData(stmt, 4, SQL_C_TYPE_TIMESTAMP, &testDate, 0, NULL);

            wchar_t dateStr[100];
            swprintf(dateStr, 100, L"%04d-%02d-%02d %02d:%02d:%02d",
                testDate.year, testDate.month, testDate.day,
                testDate.hour, testDate.minute, testDate.second);

            outfile << username << L"\t" << testId << L"\t" << score << L"\t" << dateStr << L"\n";
        }
        outfile.close();
        wcout << L"Results printed to 'test_results.txt'" << endl;
    }
};

class FeedbackManager {
public:
    void addFeedback(DatabaseManager& dbManager, const wstring& username, const wstring& feedback) {
        wstring query = L"INSERT INTO Feedback (employeeUsername, feedback, feedbackDate) VALUES ('" + username + L"', '" + feedback + L"', GETDATE())";
        if (!dbManager.executeQuery(query)) {
            wcout << L"Failed to add feedback." << endl;
        }
        else {
            wcout << L"Feedback added successfully." << endl;
        }
    }

    void viewFeedback(DatabaseManager& dbManager) {
        wstring query = L"SELECT employeeUsername, feedback, feedbackDate FROM Feedback ORDER BY feedbackDate DESC";
        if (!dbManager.executeQuery(query)) {
            wcerr << L"Failed to fetch feedback." << endl;
            return;
        }

        SQLHSTMT stmt = dbManager.getStmtHandle();
        SQLWCHAR username[255], feedback[1024];
        SQL_TIMESTAMP_STRUCT feedbackDate;

        wcout << L"Username\tFeedback\tFeedback Date" << endl;
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_WCHAR, &username, sizeof(username), NULL);
            SQLGetData(stmt, 2, SQL_C_WCHAR, &feedback, sizeof(feedback), NULL);
            SQLGetData(stmt, 3, SQL_C_TYPE_TIMESTAMP, &feedbackDate, 0, NULL);

            wchar_t dateStr[100];
            swprintf(dateStr, 100, L"%04d-%02d-%02d %02d:%02d:%02d",
                feedbackDate.year, feedbackDate.month, feedbackDate.day,
                feedbackDate.hour, feedbackDate.minute, feedbackDate.second);

            wcout << username << L"\t" << feedback << L"\t" << dateStr << endl;
        }
    }
};

class NotificationManager {
public:
    void sendNotification(DatabaseManager& dbManager, const wstring& username, const wstring& message) {
        wstring query = L"INSERT INTO Notifications (employeeUsername, notificationMessage) VALUES ('" + username + L"', '" + message + L"')";
        if (!dbManager.executeQuery(query)) {
            wcout << L"Failed to send notification." << endl;
        }
        else {
            wcout << L"Notification sent successfully." << endl;
        }
    }

    void viewNotifications(DatabaseManager& dbManager, const wstring& username) {
        wstring query = L"SELECT notificationMessage FROM Notifications WHERE employeeUsername = '" + username + L"' ORDER BY id DESC";
        if (!dbManager.executeQuery(query)) {
            wcerr << L"Failed to fetch notifications." << endl;
            return;
        }

        SQLHSTMT stmt = dbManager.getStmtHandle();
        SQLWCHAR notificationMessage[1024];

        wcout << L"Notifications for " << username << L":" << endl;
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_WCHAR, &notificationMessage, sizeof(notificationMessage), NULL);
            wcout << L"- " << notificationMessage << endl;
        }
    }
};

class Admin : public User {
private:
    DatabaseManager& dbManager;
    MaterialManager materialManager;
    QuestionManager questionManager;
    TestResultManager testResultManager;
    FeedbackManager feedbackManager;
    NotificationManager notificationManager;

public:
    Admin(const wstring& username, const wstring& password, DatabaseManager& dbMgr) : User(username, password, L"admin"), dbManager(dbMgr) {}

    void performTasks(DatabaseManager& dbManager) override {
        int choice = 0;
        do {
            wcout << L"1. Add Material\n2. Add Question\n3. Edit Material\n4. Edit Question\n5. Delete Material\n6. Delete Question\n7. View All Materials\n8. View All Questions\n9. View Test Results\n10. Print Test Results\n11. View Feedback\n12. Send Notification\n13. Exit\nEnter your choice: ";
            wcin >> choice;
            wcin.ignore();

            switch (choice) {
            case 1:
            {
                wstring topic, content;
                wcout << L"Enter material topic: ";
                getline(wcin, topic);
                wcout << L"Enter material content: ";
                getline(wcin, content);
                materialManager.addMaterial(dbManager, topic, content);
            }
            break;
            case 2:
            {
                wstring text, option1, option2, option3;
                int correctOption;
                wcout << L"Enter question text: ";
                getline(wcin, text);
                wcout << L"Enter option 1: ";
                getline(wcin, option1);
                wcout << L"Enter option 2: ";
                getline(wcin, option2);
                wcout << L"Enter option 3: ";
                getline(wcin, option3);
                wcout << L"Enter the correct option number (1-3): ";
                wcin >> correctOption;
                questionManager.addQuestion(dbManager, text, option1, option2, option3, correctOption);
            }
            break;
            case 3:
            {
                int id;
                wstring newTopic, newContent;
                wcout << L"Enter the ID of the material to edit: ";
                wcin >> id;
                wcin.ignore();
                wcout << L"Enter new material topic: ";
                getline(wcin, newTopic);
                wcout << L"Enter new material content: ";
                getline(wcin, newContent);
                materialManager.editMaterial(dbManager, id, newTopic, newContent);
            }
            break;
            case 4:
            {
                int id, newCorrectOption;
                wstring newText, newOption1, newOption2, newOption3;
                wcout << L"Enter the ID of the question to edit: ";
                wcin >> id;
                wcin.ignore();
                wcout << L"Enter new question text: ";
                getline(wcin, newText);
                wcout << L"Enter new option 1: ";
                getline(wcin, newOption1);
                wcout << L"Enter new option 2: ";
                getline(wcin, newOption2);
                wcout << L"Enter new option 3: ";
                getline(wcin, newOption3);
                wcout << L"Enter the new correct option number (1-3): ";
                wcin >> newCorrectOption;
                questionManager.editQuestion(dbManager, id, newText, newOption1, newOption2, newOption3, newCorrectOption);
            }
            break;
            case 5:
            {
                int id;
                wcout << L"Enter the ID of the material to delete: ";
                wcin >> id;
                materialManager.deleteMaterial(dbManager, id);
            }
            break;
            case 6:
            {
                int id;
                wcout << L"Enter the ID of the question to delete: ";
                wcin >> id;
                questionManager.deleteQuestion(dbManager, id);
            }
            break;
            case 7:
                materialManager.viewAllMaterials(dbManager);
                break;
            case 8:
                questionManager.viewAllQuestions(dbManager);
                break;
            case 9:
                testResultManager.viewTestResults(dbManager);
                break;
            case 10:
                testResultManager.printTestResultsToFile(dbManager);
                break;
            case 11:
                feedbackManager.viewFeedback(dbManager);
                break;
            case 12:
            {
                wstring employeeUsername, notificationMessage;
                wcout << L"Enter employee username: ";
                getline(wcin, employeeUsername);
                wcout << L"Enter notification message: ";
                getline(wcin, notificationMessage);
                notificationManager.sendNotification(dbManager, employeeUsername, notificationMessage);
            }
            break;
            case 13:
                wcout << L"Exiting..." << endl;
                return;
            default:
                wcout << L"Invalid choice. Please try again." << endl;
                break;
            }
        } while (choice != 13);
    }
};

class Employee : public User {
private:
    DatabaseManager& dbManager;
    MaterialManager materialManager;
    TestResultManager testResultManager;
    QuestionManager questionManager;
    FeedbackManager feedbackManager;
    NotificationManager notificationManager;

public:
    Employee(const wstring& username, const wstring& password, DatabaseManager& dbMgr) : User(username, password, L"employee"), dbManager(dbMgr) {}

    void performTasks(DatabaseManager& dbManager) override {
        int choice;
        do {
            wcout << L"1. View Materials\n2. Take Test\n3. View My Test Results\n4. View Notifications\n5. Submit Feedback\n6. Exit\nEnter your choice: ";
            wcin >> choice;
            wcin.ignore();

            switch (choice) {
            case 1:
                materialManager.viewAllMaterials(dbManager);
                break;
            case 2:
                takeTest(dbManager);
                break;
            case 3:
                testResultManager.viewEmployeeTestResults(dbManager, username);
                break;
            case 4:
                notificationManager.viewNotifications(dbManager, username);
                break;
            case 5:
            {
                wstring feedback;
                wcout << L"Enter your feedback: ";
                getline(wcin, feedback);
                feedbackManager.addFeedback(dbManager, username, feedback);
            }
            break;
            case 6:
                wcout << L"Exiting..." << endl;
                return;
            default:
                wcout << L"Invalid choice. Please try again." << endl;
                break;
            }
        } while (choice != 6);
    }

    void takeTest(DatabaseManager& dbManager) {
        vector<QuestionManager::Question> questions = questionManager.fetchAllQuestions(dbManager);
        int score = 0;
        for (const auto& question : questions) {
            wcout << L"Question: " << question.text << endl;
            wcout << L"1. " << question.option1 << endl;
            wcout << L"2. " << question.option2 << endl;
            wcout << L"3. " << question.option3 << endl;
            int answer;
            wcout << L"Enter your answer (1-3): ";
            wcin >> answer;
            if (answer == question.correctAnswer) {
                score++;
            }
        }
        wcout << L"Your score: " << score << L" out of " << questions.size() << endl;
        testResultManager.saveTestResults(dbManager, username, 1, score);  // Assuming testId is 1 for simplicity
    }
};

int main() {
    DatabaseManager dbManager;

    wstring enteredUsername, enteredPassword;
    bool isAuthenticated = false;

    do {
        wcout << L"Enter username: ";
        wcin >> enteredUsername;
        wcout << L"Enter password: ";
        wcin >> enteredPassword;
        wcin.ignore(1000, L'\n');

        wstring userType;
        User* user = nullptr;

        if (dbManager.authenticateUser(enteredUsername, enteredPassword, userType)) {
            if (userType == L"admin") {
                user = new Admin(enteredUsername, enteredPassword, dbManager);
            }
            else if (userType == L"employee") {
                user = new Employee(enteredUsername, enteredPassword, dbManager);
            }

            if (user) {
                wcout << (userType == L"admin" ? L"Admin" : L"Employee") << L" logged in successfully." << endl;
                user->performTasks(dbManager);
                isAuthenticated = true;
                delete user;
            }
        }
        else {
            wcout << L"Invalid credentials. Access denied." << endl;
        }
    } while (!isAuthenticated);

    return 0;
}

