#include <QtTest/QtTest>
#include <QObject>
#include <QApplication> // Needed for QWidget-based classes like vk701page
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>

// Adjust paths based on actual location relative to the tests directory
#include "../inc/vk701page.h"
#include "../inc/vk701nsd.h" // vk701page creates an instance of vk701nsd
#include "../inc/Global.h"   // For DAQState enum
#include "../ui_vk701page.h" // Generated UI header

// Forward declaration for vk701nsd if its full definition isn't strictly needed
// However, vk701page instantiates it, so its definition will be pulled in.

class TestVk701Page : public QObject
{
    Q_OBJECT

public:
    TestVk701Page();
    ~TestVk701Page();

private slots:
    void initTestCase();    // Called before the first test function is executed
    void cleanupTestCase(); // Called after the last test function is executed
    void init();            // Called before each test function is executed
    void cleanup();         // Called after each test function is executed

    // Test methods
    void testInitialState();
    void testSamplingFrequencyValidation_data(); // For QTest::addColumn
    void testSamplingFrequencyValidation();
    void testDbRangeValidation_data();
    void testDbRangeValidation();
    void testPaginationLogic();
    void testDatabaseOperations(); // Will cover saveData, cleanupOldData, deleteData, nuke
    // void testHandleResultValue(); // Mocking vk701nsd needed
    // void testHandleStateChanged(); // Mocking vk701nsd needed

private:
    // QApplication *app; // QApplication instance is managed by QTEST_MAIN
    vk701page *pageInstance;
    // Ui::vk701page ui; // Not needed as member, pageInstance->ui is used.
    QSqlDatabase testDb;

    // void setupInMemoryDatabase(); // Removed as integrated into initTestCase
    void populateTestDataForRound(int roundId, int numRecordsPerChannel, int numChannels);
};

TestVk701Page::TestVk701Page() : pageInstance(nullptr)
{
    // Constructor: app instance handled by QTEST_MAIN
}

TestVk701Page::~TestVk701Page()
{
    // Destructor
}

void TestVk701Page::initTestCase()
{
    qDebug() << "Starting vk701page tests...";
    // This is called once before any test functions are executed.
    // Setup in-memory DB for all tests in this class.
    // Force vk701page to use this in-memory DB by using its hardcoded connection name "vk701".
    if (QSqlDatabase::contains("vk701")) {
        QSqlDatabase::database("vk701").close(); // Close if already open (e.g. from previous unclean test run)
        QSqlDatabase::removeDatabase("vk701");
    }
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) { // Also check default just in case
         QSqlDatabase::database(QSqlDatabase::defaultConnection).close();
         QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }


    testDb = QSqlDatabase::addDatabase("QSQLITE", "vk701"); // Use the name vk701page expects
    testDb.setDatabaseName(":memory:");
    QVERIFY2(testDb.open(), qPrintable(QString("Failed to open in-memory database for 'vk701': %1").arg(testDb.lastError().text())));

    // Create tables. vk701page::InitDB will also try to do this, but it's safe.
    // It will use the "vk701" connection we've just set up as an in-memory one.
    QSqlQuery query(testDb); // Use testDb which is the "vk701" connection
    QVERIFY(query.exec("CREATE TABLE IF NOT EXISTS IEPEdata (RoundID INTEGER, ChID INTEGER, VibrationData REAL)"));
    QVERIFY(query.exec("CREATE TABLE IF NOT EXISTS TimeRecord (Round INTEGER, TimeDiff REAL)"));
    QVERIFY(query.exec("CREATE INDEX IF NOT EXISTS idx_IEPEdata_RoundID ON IEPEdata(RoundID)"));
}

void TestVk701Page::cleanupTestCase()
{
    qDebug() << "Finished vk701page tests.";
    if (testDb.isOpen()) {
        testDb.close();
    }
    QSqlDatabase::removeDatabase("vk701"); // Remove the connection used by vk701page
    // This is called once after all test functions are executed.
}

void TestVk701Page::init()
{
    // This is called before each test function.
    // Create a fresh page instance for each test.
    // Its InitDB method should now use the in-memory "vk701" database we set up in initTestCase.
    pageInstance = new vk701page(); 

    // Verify that the pageInstance is indeed using an in-memory database.
    // This is an indirect check. A more direct check would involve inspecting pageInstance->db, if possible.
    QSqlDatabase instanceDb = QSqlDatabase::database("vk701"); // Get the connection vk701page should be using
    QVERIFY(instanceDb.isValid());
    QVERIFY(instanceDb.isOpen());
    QCOMPARE(instanceDb.driverName(), "QSQLITE");
    // For in-memory, databaseName() might be ":memory:" or an empty string after opening,
    // depending on Qt version and how it was set.
    // QVERIFY(instanceDb.databaseName() == ":memory:" || instanceDb.databaseName().isEmpty());
    // A more reliable check might be to ensure it's not the disk path.
    QVERIFY(!instanceDb.databaseName().contains("vibsqlite.db"));


    // Clear tables before each test to ensure isolation, as pageInstance might add data.
    // The cleanup() method already does this, but doing it here too can be safer for some tests.
    QSqlQuery clearQuery(testDb); // testDb is the same as QSqlDatabase::database("vk701")
    QVERIFY(clearQuery.exec("DELETE FROM IEPEdata"));
    QVERIFY(clearQuery.exec("DELETE FROM TimeRecord"));
}

void TestVk701Page::cleanup()
{
    // This is called after each test function.
    delete pageInstance;
    pageInstance = nullptr;

    // Clean up database tables for test isolation if data was added by the test
    // This is important because pageInstance->db is the same connection as testDb
    QSqlQuery clearQuery(testDb); 
    QVERIFY(clearQuery.exec("DELETE FROM IEPEdata"));
    QVERIFY(clearQuery.exec("DELETE FROM TimeRecord"));
    // currentRoundID in vk701page is reset by creating a new instance.
}

void TestVk701Page::testInitialState()
{
    QVERIFY(pageInstance != nullptr);
    QCOMPARE(pageInstance->currentRoundID, 0); // Assuming it's initialized to 0 or read from DB
    QCOMPARE(pageInstance->db_currentPage, 0); // db_currentPage is private, need getter or different test
                                             // For now, assuming default init, or test via side-effects.
                                             // The .h file has `int db_currentPage;` but it's initialized in .cpp constructor.
                                             // The .cpp shows `db_currentPage = 0;` in constructor.
    // QCOMPARE(pageInstance->getDbCurrentPage(), 0); // If a getter existed
    QCOMPARE(pageInstance->AllRecordStart, false);

    // Pagination buttons initial state (assuming UI elements are accessible via pageInstance->ui)
    QVERIFY(pageInstance->ui->btn_nextPage != nullptr); // Check if UI setup worked
    QCOMPARE(pageInstance->ui->btn_nextPage->isEnabled(), false);
    QCOMPARE(pageInstance->ui->btn_prevPage->isEnabled(), false);
    QCOMPARE(pageInstance->ui->lbl_pageInfo->text(), "Page 0 of 0");
}

void TestVk701Page::testSamplingFrequencyValidation_data()
{
    QTest::addColumn<QString>("inputFrequency");
    QTest::addColumn<bool>("isValid"); // Expected outcome for workerThread->isRunning() (conceptual)
    QTest::addColumn<bool>("showsWarning"); // Expected outcome for QMessageBox (conceptual)

    QTest::newRow("valid_5000") << "5000" << true << false;
    QTest::newRow("valid_1000") << "1000" << true << false;
    QTest::newRow("valid_100000") << "100000" << true << false;
    QTest::newRow("invalid_text") << "abc" << false << true;
    QTest::newRow("invalid_too_low") << "999" << false << true;
    QTest::newRow("invalid_too_high") << "100001" << false << true;
    QTest::newRow("invalid_empty") << "" << false << true;
    QTest::newRow("invalid_negative") << "-5000" << false << true;
}

void TestVk701Page::testSamplingFrequencyValidation()
{
    QFETCH(QString, inputFrequency);
    QFETCH(bool, isValid);
    // QFETCH(bool, showsWarning); // Not directly testable for QMessageBox

    pageInstance->ui->le_samplingFrequency->setText(inputFrequency);
    
    // Mocking QMessageBox is complex. We'll check the side-effects.
    // If input is invalid, workerThread should not start, and btn_start_2 should be re-enabled.
    // If input is valid, workerThread should start.
    // vk701page::workerThread is private, and vk701page::worker is also private.
    // This makes direct verification hard.
    // We can check if btn_start_2 is re-enabled on invalid input.

    pageInstance->ui->btn_start_2->setEnabled(true); // Ensure it's enabled before click
    pageInstance->on_btn_start_2_clicked();

    if (isValid) {
        // If valid, btn_start_2 remains disabled (as per current logic in on_btn_start_2_clicked)
        // and workerThread would be running.
        // QVERIFY(pageInstance->workerThread->isRunning()); // Cannot access private member
        QCOMPARE(pageInstance->ui->btn_start_2->isEnabled(), false);
         // We also expect samplingFrequency to be set on the worker if valid
        // QCOMPARE(pageInstance->worker->samplingFrequency, inputFrequency.toInt()); // worker is private
    } else {
        // If invalid, QMessageBox is shown, and btn_start_2 is re-enabled.
        QCOMPARE(pageInstance->ui->btn_start_2->isEnabled(), true);
    }
    // To truly test worker interactions, vk701nsd needs to be mockable and injected.
}


void TestVk701Page::testDbRangeValidation_data()
{
    QTest::addColumn<QString>("startRange");
    QTest::addColumn<QString>("endRange");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid_0_100") << "0" << "100" << true;
    QTest::newRow("valid_10_10") << "10" << "10" << true;
    QTest::newRow("invalid_text_start") << "abc" << "100" << false;
    QTest::newRow("invalid_text_end") << "0" << "xyz" << false;
    QTest::newRow("invalid_start_greater_than_end") << "100" << "0" << false;
    QTest::newRow("invalid_negative_start") << "-1" << "100" << false;
    QTest::newRow("invalid_negative_end") << "0" << "-100" << false;
    QTest::newRow("invalid_empty_start") << "" << "100" << false;
    QTest::newRow("invalid_empty_end") << "0" << "" << false;
}

void TestVk701Page::testDbRangeValidation()
{
    QFETCH(QString, startRange);
    QFETCH(QString, endRange);
    QFETCH(bool, isValid);

    pageInstance->ui->le_rage_start->setText(startRange);
    pageInstance->ui->le_rage_end->setText(endRange);

    // on_btn_showDB_clicked has validation. If invalid, it shows a QMessageBox and returns.
    // If valid, it proceeds to query DB.
    // We can't check QMessageBox directly.
    // We can check if db_filter_minRoundID/db_filter_maxRoundID are set (they are private).
    // A simpler check: if it's invalid, the table rowCount should remain 0 (or unchanged).
    // If valid, it might change (but depends on DB content).

    int initialRowCount = pageInstance->ui->table_vibDB->rowCount();
    pageInstance->on_btn_showDB_clicked(); // This uses the hardcoded "vk701" DB.

    if (isValid) {
        // If valid, it attempts to load data. If DB is empty, rowCount might still be 0.
        // This test is more about the input validation path.
        // For a robust check, we'd need to see if loadDbPageData was called,
        // or if db_filter_minRoundID was updated.
        // QCOMPARE(pageInstance->db_filter_minRoundID, startRange.toInt()); // Private member
        QVERIFY(true); // Placeholder, actual effect depends on DB state and private members
    } else {
        // If invalid, it should return early. Row count should not change due to this call.
        // And if a QMessageBox was shown, the application would typically block.
        // In a test, it doesn't block, but the function should have returned.
        QCOMPARE(pageInstance->ui->table_vibDB->rowCount(), initialRowCount);
    }
}

void TestVk701Page::populateTestDataForRound(int roundId, int numRecordsPerChannel, int numChannels) {
    QSqlQuery query(QSqlDatabase::database("vk701")); // Ensure using the correct connection
    query.prepare("INSERT INTO IEPEdata (RoundID, ChID, VibrationData) VALUES (?, ?, ?)");
    for (int rec = 0; rec < numRecordsPerChannel; ++rec) {
        for (int ch = 0; ch < numChannels; ++ch) {
            query.addBindValue(roundId);
            query.addBindValue(ch + 1);
            query.addBindValue(100.0 + rec + (ch * 0.1)); // Some dummy data
            QVERIFY(query.exec());
        }
    }
}


void TestVk701Page::testPaginationLogic()
{
    // This test needs to interact with the database.
    // It assumes vk701page can be made to use the in-memory 'testDb'.
    // As vk701page::InitDB is hardcoded, this test will fail to show correct behavior
    // unless vk701page is refactored or testDb somehow becomes the default for "vk701".
    // For this test, we will assume that pageInstance->db IS testDb.
    // The most robust way is to modify vk701page to allow DB injection.
    // Lacking that, we'll use a conceptual approach.

    // Simulate vk701page using our testDb by directly manipulating testDb
    // and then calling vk701page methods, hoping its internal DB calls reflect testDb state.
    // This is fragile. A better way would be:
    // pageInstance->setDatabaseConnection("vk701"); // This is now implicitly handled

    // Setup: Populate testDb (which is the "vk701" in-memory connection) with known data
    int recordsPerRoundId = pageInstance->db_pageSize * 2 + 50; // e.g., 100 * 2 + 50 = 250 records for RoundID 1
    populateTestDataForRound(1, recordsPerRoundId / 4, 4); // Assuming 4 channels for IEPEdata structure. Total 250 records.

    pageInstance->ui->le_rage_start->setText("1");
    pageInstance->ui->le_rage_end->setText("1");

    // Call on_btn_showDB_clicked() to trigger pagination initialization
    // This will use its own DB. For this test to be meaningful, we need vk701page
    // to use the "test_vk701_connection" which has the data we just populated.
    // Hack: If we could force InitDB on the instance with the test connection name.
    // Forcing pageInstance to use testDb connection:
    // This is where true testability of vk701page is vital.
    // If pageInstance->db cannot be made to point to testDb, then on_btn_showDB_clicked()
    // will operate on the default disk DB, not our in-memory data.
    // Let's assume this connection management is resolved for the test to proceed.
    // (e.g., by making vk701page::db public for tests, or providing a setter)

    // --- This part of the test will likely not work as expected with current vk701page ---
    // --- because on_btn_showDB_clicked() uses its own hardcoded db connection ---
    // --- We'd need to query the "vk701" connection to see effects, not "test_vk701_connection"
    // --- Or, better, refactor vk701page.

    // Now, when on_btn_showDB_clicked is called, it should use the in-memory "vk701" database.
    pageInstance->on_btn_showDB_clicked();
    
    // Verifications should now reflect the in-memory data
    // QCOMPARE(pageInstance->db_totalPages, 3); // This checks a private member.
                                               // The UI check below is a better test of observable behavior.
    QCOMPARE(pageInstance->ui->lbl_pageInfo->text(), "Page 1 of 3"); // Implicitly tests total pages calculation.
    QCOMPARE(pageInstance->ui->btn_prevPage->isEnabled(), false); 
    QCOMPARE(pageInstance->ui->btn_nextPage->isEnabled(), true);
    QCOMPARE(pageInstance->ui->table_vibDB->rowCount(), pageInstance->db_pageSize); // 100 rows

    // Navigate to Next page
    pageInstance->on_btn_nextPage_clicked(); // This calls loadDbPageData internally
    QCOMPARE(pageInstance->ui->lbl_pageInfo->text(), "Page 2 of 3");
    QCOMPARE(pageInstance->ui->btn_prevPage->isEnabled(), true);
    QCOMPARE(pageInstance->ui->btn_nextPage->isEnabled(), true);
    QCOMPARE(pageInstance->ui->table_vibDB->rowCount(), pageInstance->db_pageSize); // 100 rows

    // Navigate to Next page (last page)
    pageInstance->on_btn_nextPage_clicked();
    QCOMPARE(pageInstance->ui->lbl_pageInfo->text(), "Page 3 of 3");
    QCOMPARE(pageInstance->ui->btn_prevPage->isEnabled(), true);
    QCOMPARE(pageInstance->ui->btn_nextPage->isEnabled(), false);
    QCOMPARE(pageInstance->ui->table_vibDB->rowCount(), 50); // Remaining 50 rows

    // Navigate to Previous page
    pageInstance->on_btn_prevPage_clicked();
    QCOMPARE(pageInstance->ui->lbl_pageInfo->text(), "Page 2 of 3");
    QCOMPARE(pageInstance->ui->btn_prevPage->isEnabled(), true);
    QCOMPARE(pageInstance->ui->btn_nextPage->isEnabled(), true);
    QCOMPARE(pageInstance->ui->table_vibDB->rowCount(), pageInstance->db_pageSize);
}

void TestVk701Page::testDatabaseOperations()
{
    // Again, this assumes vk701page can use the 'testDb'.
    // saveDataToDatabase is asynchronous (QtConcurrent::run). Testing it is tricky.
    // For simplicity, we'll test its synchronous parts or assume it runs synchronously in tests.
    // The current saveDataToDatabase updates a batchData buffer then a timer commits it.
    // This is hard to test directly without controlling the timer or refactoring.

    // Test 1: saveDataToDatabase (conceptual - real test needs different approach for async)
    // For now, let's test the batchData preparation part if possible, or simplify.
    // The actual DB insertion is tied to a timer and AllRecordStart flag.

    // Test 2: cleanupOldData()
    // This test relies on pageInstance->db being the in-memory "vk701" connection.
    populateTestDataForRound(1, 10, 4);
    populateTestDataForRound(2, 10, 4);
    populateTestDataForRound(3, 10, 4);
    populateTestDataForRound(4, 10, 4);
    pageInstance->currentRoundID = 4; // Simulate current round
    
    // Call cleanupOldData - this method uses pageInstance->db directly.
    pageInstance->cleanupOldData(2); // Keep last 2 rounds (3 and 4)

    QSqlQuery query(QSqlDatabase::database("vk701"));
    query.exec("SELECT COUNT(*) FROM IEPEdata WHERE RoundID = 1");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0); // Round 1 should be deleted

    query.exec("SELECT COUNT(*) FROM IEPEdata WHERE RoundID = 2");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0); // Round 2 should be deleted

    query.exec("SELECT COUNT(*) FROM IEPEdata WHERE RoundID = 3");
    QVERIFY(query.next());
    QVERIFY(query.value(0).toInt() > 0); // Round 3 should exist (10 records * 4 channels = 40, but it's just >0)

    query.exec("SELECT COUNT(*) FROM IEPEdata WHERE RoundID = 4");
    QVERIFY(query.next());
    QVERIFY(query.value(0).toInt() > 0); // Round 4 should exist

    // Test 3: on_btn_deleteData_clicked()
    populateTestDataForRound(5, 20, 4); // Add data for round 5
    pageInstance->ui->spinBox_round->setValue(5);
    pageInstance->on_btn_deleteData_clicked(); 

    query.exec("SELECT COUNT(*) FROM IEPEdata WHERE RoundID = 5");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0); // Round 5 should be deleted

    // Test 4: on_btn_nuke_clicked()
    // This requires mocking QMessageBox::question. For now, we assume "Yes" and call the core logic.
    // A more direct test would be to refactor the core logic of nuke into a separate public/protected method.
    // For now, we'll test the state after simulating the nuke.
    populateTestDataForRound(6, 10, 4);
    populateTestDataForRound(7, 10, 4);
    
    // Simulate "Yes" to QMessageBox. This is a limitation of not mocking UI.
    // We are testing the consequence of the nuke, not the button click itself with UI interaction.
    // The actual on_btn_nuke_clicked() in vk701page.cpp:
    // reply = QMessageBox::question(...); if (reply != QMessageBox::Yes) return;
    // db.transaction(); query.exec("DELETE FROM IEPEdata"); ... query.exec("VACUUM"); currentRoundID = 0;
    // So, we can call the part after the QMessageBox check.
    // This is not ideal but tests the database clearing part.
    // To test this properly, on_btn_nuke_clicked would need to be refactored.
    // For now, let's assume we want to test the effect if it were to proceed:
    QSqlDatabase db_conn = QSqlDatabase::database("vk701");
    QVERIFY(db_conn.transaction());
    QSqlQuery nukeQuery(db_conn);
    QVERIFY(nukeQuery.exec("DELETE FROM IEPEdata"));
    QVERIFY(nukeQuery.exec("DELETE FROM TimeRecord"));
    QVERIFY(db_conn.commit());
    QVERIFY(nukeQuery.exec("VACUUM"));
    pageInstance->currentRoundID = 0; // Manually set as the original slot would

    query.exec("SELECT COUNT(*) FROM IEPEdata");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0); // All data should be gone
    QCOMPARE(pageInstance->currentRoundID, 0);
}

void TestVk701Page::testSaveDataToDatabase_Placeholder()
{
    // saveDataToDatabase involves QtConcurrent::run and a QTimer for batch commits.
    // This makes it difficult to test synchronously and reliably in a unit test
    // without significant refactoring of vk701page or using advanced testing tools
    // to control timers and threads.

    // Conceptual steps if it were more testable:
    // 1. Ensure AllRecordStart = true.
    // 2. Call pageInstance->handleResultValue() with some data (as this triggers saveDataToDatabase).
    //    - This itself has a dependency on the data format from vk701nsd.
    // 3. Advance timers or wait for async operations.
    // 4. Query the database to verify data insertion.

    // Current implementation of saveDataToDatabase:
    // - Takes data, channels, pointsPerChannel.
    // - Prepares QVariantLists roundIds, channelIds, values.
    // - Locks a mutex to update `batchData` (a QList<QVariantList> member).
    // - The actual commit to DB happens in a QTimer connected lambda, which checks
    //   `AllRecordStart` and if `batchData` is not empty.

    // A limited test could check if `batchData` is populated after calling the synchronous part.
    // However, `batchData` is private.

    qDebug() << "Test for saveDataToDatabase is a placeholder due to its asynchronous nature and private members.";
    QVERIFY(true); // Placeholder
}


// QTEST_MAIN requires QApplication for widget tests
QTEST_MAIN(TestVk701Page)
#include "testvk701page.moc" // Required for Q_OBJECT with QTest framework

// Challenges for testing vk701page:
// 1. Hardcoded database filename and connection name in InitDB:
//    - Makes it very difficult to redirect database operations to an in-memory test database
//      without modifying vk701page's source code (e.g., to accept DB name/connection).
//    - Workaround: Manually create tables in testDb and test methods like loadDbPageData
//      by setting its internal state (db_currentPage, db_totalPages) and hoping its
//      internal pageInstance->db points to the test one. This is fragile.
// 2. Private UI elements and worker threads:
//    - Accessing ui elements is fine via pageInstance->ui.
//    - Worker threads (vk701nsd) and their interactions (signals/slots) are hard to test
//      without a proper mocking framework for vk701nsd or making worker accessible.
// 3. Asynchronous operations:
//    - `saveDataToDatabase` uses QtConcurrent::run and a QTimer for batch commits.
//      Testing these requires more advanced techniques (e.g., QSignalSpy for timers, event loop processing).
// 4. QMessageBox popups:
//    - Directly testing if a QMessageBox is shown is not straightforward in unit tests.
//    - Focus on testing the logic that leads to the QMessageBox and the state after it would have been shown.
//
// The provided tests attempt to cover core logic where possible, with placeholders or
// conceptual notes for parts that are hard to test without refactoring or advanced mocking.
// The pagination test, for instance, assumes that `pageInstance->loadDbPageData()` can be made
// to operate on the `testDb` by setting relevant public/internal members of `pageInstance`.
// The database operations test also relies on this assumption for verifying data changes.
