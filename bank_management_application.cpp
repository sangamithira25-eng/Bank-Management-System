#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

static const char XOR_KEY[] = "BankSecure2026";

std::string xorCipher(const std::string &input) {
    std::string output;
    output.reserve(input.size());
    size_t keyLen = sizeof(XOR_KEY) - 1;
    for (size_t i = 0; i < input.size(); ++i) {
        output.push_back(input[i] ^ XOR_KEY[i % keyLen]);
    }
    return output;
}

std::string toHex(const std::string &data) {
    static const char *hexDigits = "0123456789ABCDEF";
    std::string result;
    result.reserve(data.size() * 2);
    for (unsigned char ch : data) {
        result.push_back(hexDigits[ch >> 4]);
        result.push_back(hexDigits[ch & 0x0F]);
    }
    return result;
}

std::string fromHex(const std::string &hex) {
    std::string result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        auto fromHexDigit = [](char ch) {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            return 0;
        };
        unsigned char high = fromHexDigit(hex[i]);
        unsigned char low = fromHexDigit(hex[i + 1]);
        result.push_back(static_cast<char>((high << 4) | low));
    }
    return result;
}

void clearInput() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

std::string sanitizeField(const std::string &field) {
    std::string result = field;
    for (char &ch : result) {
        if (ch == '\t' || ch == '\n' || ch == '\r') {
            ch = ' ';
        }
    }
    return result;
}

struct Account {
    int id;
    std::string name;
    double balance;
    std::string encryptedPin;
    bool isActive;

    static std::string encryptPin(const std::string &pin) {
        return toHex(xorCipher(pin));
    }

    bool verifyPin(const std::string &pin) const {
        return encryptedPin == encryptPin(pin);
    }

    std::string serialize() const {
        std::ostringstream out;
        out << id << '\t'
            << sanitizeField(name) << '\t'
            << std::fixed << std::setprecision(2) << balance << '\t'
            << encryptedPin << '\t'
            << (isActive ? "1" : "0");
        std::string line = out.str();
        return toHex(xorCipher(line));
    }

    static Account deserialize(const std::string &hexLine) {
        std::string decrypted = xorCipher(fromHex(hexLine));
        std::istringstream in(decrypted);
        Account account;
        std::string activeFlag;
        in >> account.id;
        in.ignore(1, '\t');
        std::getline(in, account.name, '\t');
        in >> account.balance;
        in.ignore(1, '\t');
        std::getline(in, account.encryptedPin, '\t');
        std::getline(in, activeFlag);
        account.isActive = (activeFlag == "1");
        return account;
    }
};

class Bank {
public:
    explicit Bank(const std::string &dataFile) : dataFile_(dataFile), nextAccountId_(1001) {
        loadAccounts();
    }

    ~Bank() {
        saveAccounts();
    }

    void createAccount() {
        std::string name;
        double initialDeposit = 0.0;
        std::string pin;
        std::string confirmedPin;

        std::cout << "Enter account holder name: ";
        std::getline(std::cin, name);
        if (name.empty()) {
            std::cout << "Name cannot be empty.\n";
            return;
        }

        std::cout << "Enter initial deposit: ";
        while (!(std::cin >> initialDeposit) || initialDeposit < 0) {
            std::cout << "Enter a valid deposit amount: ";
            clearInput();
        }
        clearInput();

        std::cout << "Choose a 4-digit PIN: ";
        std::getline(std::cin, pin);
        std::cout << "Confirm PIN: ";
        std::getline(std::cin, confirmedPin);
        if (pin != confirmedPin || pin.size() != 4 || !std::all_of(pin.begin(), pin.end(), ::isdigit)) {
            std::cout << "PIN must be a matching 4-digit number.\n";
            return;
        }

        Account account;
        account.id = nextAccountId_++;
        account.name = name;
        account.balance = initialDeposit;
        account.encryptedPin = Account::encryptPin(pin);
        account.isActive = true;
        accounts_.push_back(account);
        saveAccounts();

        std::cout << "Account created successfully. Your account number is " << account.id << ".\n";
    }

    void login() {
        int accountId;
        std::string pin;
        std::cout << "Enter account number: ";
        if (!(std::cin >> accountId)) {
            std::cout << "Invalid account number.\n";
            clearInput();
            return;
        }
        clearInput();
        std::cout << "Enter PIN: ";
        std::getline(std::cin, pin);

        Account *account = findAccount(accountId);
        if (account == nullptr || !account->isActive || !account->verifyPin(pin)) {
            std::cout << "Login failed. Check account number and PIN.\n";
            return;
        }

        std::cout << "Login successful. Welcome, " << account->name << "!\n";
        accountMenu(*account);
    }

    void showAllAccounts() const {
        std::cout << "\nStored Accounts (Admin View)\n";
        if (accounts_.empty()) {
            std::cout << "No accounts available.\n";
            return;
        }
        for (const auto &account : accounts_) {
            std::cout << "ID: " << account.id
                      << " | Name: " << account.name
                      << " | Balance: $" << std::fixed << std::setprecision(2) << account.balance
                      << " | Active: " << (account.isActive ? "Yes" : "No") << "\n";
        }
    }

private:
    std::vector<Account> accounts_;
    std::string dataFile_;
    int nextAccountId_;

    void saveAccounts() const {
        std::ofstream file(dataFile_, std::ios::trunc);
        if (!file) {
            std::cout << "Unable to save account data.\n";
            return;
        }
        for (const auto &account : accounts_) {
            file << account.serialize() << '\n';
        }
    }

    void loadAccounts() {
        std::ifstream file(dataFile_);
        if (!file) {
            return;
        }
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) {
                continue;
            }
            try {
                Account account = Account::deserialize(line);
                accounts_.push_back(account);
                if (account.id >= nextAccountId_) {
                    nextAccountId_ = account.id + 1;
                }
            } catch (...) {
                std::cout << "Warning: failed to parse saved account record.\n";
            }
        }
    }

    Account *findAccount(int accountId) {
        for (auto &account : accounts_) {
            if (account.id == accountId) {
                return &account;
            }
        }
        return nullptr;
    }

    void accountMenu(Account &account) {
        bool activeSession = true;
        while (activeSession) {
            std::cout << "\n1. Check balance\n"
                      << "2. Deposit\n"
                      << "3. Withdraw\n"
                      << "4. Change PIN\n"
                      << "5. Logout\n"
                      << "Choose an option: ";
            int choice;
            if (!(std::cin >> choice)) {
                std::cout << "Invalid choice.\n";
                clearInput();
                continue;
            }
            clearInput();

            switch (choice) {
                case 1:
                    std::cout << "Account balance: $" << std::fixed << std::setprecision(2) << account.balance << "\n";
                    break;
                case 2:
                    performDeposit(account);
                    break;
                case 3:
                    performWithdrawal(account);
                    break;
                case 4:
                    changePin(account);
                    break;
                case 5:
                    activeSession = false;
                    std::cout << "Logged out successfully.\n";
                    break;
                default:
                    std::cout << "Please choose a valid option between 1 and 5.\n";
                    break;
            }
        }
        saveAccounts();
    }

    void performDeposit(Account &account) {
        double amount;
        std::cout << "Enter deposit amount: ";
        if (!(std::cin >> amount) || amount <= 0) {
            std::cout << "Invalid amount.\n";
            clearInput();
            return;
        }
        account.balance += amount;
        std::cout << "Deposit successful. New balance: $" << std::fixed << std::setprecision(2) << account.balance << "\n";
        clearInput();
    }

    void performWithdrawal(Account &account) {
        double amount;
        std::cout << "Enter withdrawal amount: ";
        if (!(std::cin >> amount) || amount <= 0) {
            std::cout << "Invalid amount.\n";
            clearInput();
            return;
        }
        if (amount > account.balance) {
            std::cout << "Insufficient funds. Current balance: $" << std::fixed << std::setprecision(2) << account.balance << "\n";
            clearInput();
            return;
        }
        account.balance -= amount;
        std::cout << "Withdrawal successful. New balance: $" << std::fixed << std::setprecision(2) << account.balance << "\n";
        clearInput();
    }

    void changePin(Account &account) {
        std::string currentPin;
        std::string newPin;
        std::string confirmPin;
        std::cout << "Enter current PIN: ";
        std::getline(std::cin, currentPin);
        if (!account.verifyPin(currentPin)) {
            std::cout << "Current PIN is incorrect.\n";
            return;
        }
        std::cout << "Enter new 4-digit PIN: ";
        std::getline(std::cin, newPin);
        std::cout << "Confirm new PIN: ";
        std::getline(std::cin, confirmPin);
        if (newPin != confirmPin || newPin.size() != 4 || !std::all_of(newPin.begin(), newPin.end(), ::isdigit)) {
            std::cout << "New PIN must match and be a 4-digit number.\n";
            return;
        }
        account.encryptedPin = Account::encryptPin(newPin);
        std::cout << "PIN changed successfully.\n";
    }
};

void showMainMenu() {
    std::cout << "\nSecure Bank Management System\n"
              << "1. Open new account\n"
              << "2. Login to existing account\n"
              << "3. Exit\n"
              << "Choose an option: ";
}

int main() {
    Bank bank("bank_data.enc");
    bool running = true;

    while (running) {
        showMainMenu();
        int choice;
        if (!(std::cin >> choice)) {
            std::cout << "Invalid input. Please enter a number.\n";
            clearInput();
            continue;
        }
        clearInput();

        switch (choice) {
            case 1:
                bank.createAccount();
                break;
            case 2:
                bank.login();
                break;
            case 3:
                running = false;
                std::cout << "Exiting secure bank application.\n";
                break;
            default:
                std::cout << "Please choose a valid option between 1 and 3.\n";
                break;
        }
    }
    return 0;
}
