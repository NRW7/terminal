// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "..\..\interactivity\onecore\SystemConfigurationProvider.hpp"

// some assumptions have been made on this value. only change it if you have a good reason to.
#define NUMBER_OF_SCENARIO_INPUTS 10
#define READ_BATCH 3

using WEX::Logging::Log;
using namespace WEX::Common;
using namespace WEX::TestExecution;

// This class is intended to test:
// FlushConsoleInputBuffer
// PeekConsoleInput
// ReadConsoleInput
// WriteConsoleInput
// GetNumberOfConsoleInputEvents
// GetNumberOfConsoleMouseButtons
// ReadConsoleA
class InputTests
{
    BEGIN_TEST_CLASS(InputTests)
    END_TEST_CLASS()

    TEST_CLASS_SETUP(TestSetup);
    TEST_CLASS_CLEANUP(TestCleanup);

    // note: GetNumberOfConsoleMouseButtons crashes with nullptr, so there's no negative test
    TEST_METHOD(TestGetMouseButtonsValid);

    TEST_METHOD(TestInputScenario);
    TEST_METHOD(TestFlushValid);
    TEST_METHOD(TestFlushInvalid);
    TEST_METHOD(TestPeekConsoleInvalid);
    TEST_METHOD(TestReadConsoleInvalid);
    TEST_METHOD(TestWriteConsoleInvalid);

    TEST_METHOD(TestReadWaitOnHandle);

    TEST_METHOD(TestReadConsolePasswordScenario);

    TEST_METHOD(TestMouseWheelReadConsoleMouseInput);
    TEST_METHOD(TestMouseHorizWheelReadConsoleMouseInput);
    TEST_METHOD(TestMouseWheelReadConsoleNoMouseInput);
    TEST_METHOD(TestMouseHorizWheelReadConsoleNoMouseInput);
    TEST_METHOD(TestMouseWheelReadConsoleInputQuickEdit);
    TEST_METHOD(TestMouseHorizWheelReadConsoleInputQuickEdit);
    TEST_METHOD(RawReadUnpacksCoalescedInputRecords);

    BEGIN_TEST_METHOD(TestVtInputGeneration)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD();

    BEGIN_TEST_METHOD(TestCookedAliasProcessing)
        TEST_METHOD_PROPERTY(L"TestTimeout", L"00:01:00")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestCookedTextEntry)
        TEST_METHOD_PROPERTY(L"TestTimeout", L"00:01:00")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestCookedAlphaPermutations)
        TEST_METHOD_PROPERTY(L"TestTimeout", L"00:01:00")
        TEST_METHOD_PROPERTY(L"Data:inputcp", L"{437, 932}")
        TEST_METHOD_PROPERTY(L"Data:outputcp", L"{437, 932}")
        TEST_METHOD_PROPERTY(L"Data:inputmode", L"{487, 481}") // 487 is 0x1e7, 485 is 0x1e1 (ENABLE_LINE_INPUT on/off)
        TEST_METHOD_PROPERTY(L"Data:outputmode", L"{7}")
        TEST_METHOD_PROPERTY(L"Data:font", L"{Consolas, MS Gothic}")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestCookedReadCharByChar)
        //TEST_METHOD_PROPERTY(L"TestTimeout", L"00:01:00")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestCookedReadLeadTrailString)
        //TEST_METHOD_PROPERTY(L"TestTimeout", L"00:01:00")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestCookedReadChangeCodepageInMiddle)
        //TEST_METHOD_PROPERTY(L"TestTimeout", L"00:01:00")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestCookedReadChangeCodepageBetweenBytes)
        //TEST_METHOD_PROPERTY(L"TestTimeout", L"00:01:00")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestDirectReadCharByChar)
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestDirectReadLeadTrailString)
    END_TEST_METHOD()
};

void VerifyNumberOfInputRecords(const HANDLE hConsoleInput, _In_ DWORD nInputs)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    DWORD nInputEvents = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(hConsoleInput, &nInputEvents));
    VERIFY_ARE_EQUAL(nInputEvents,
                     nInputs,
                     L"Verify number of input events");
}

bool InputTests::TestSetup()
{
    const bool fRet = Common::TestBufferSetup();

    HANDLE hConsoleInput = GetStdInputHandle();
    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hConsoleInput));
    VerifyNumberOfInputRecords(hConsoleInput, 0);

    return fRet;
}

bool InputTests::TestCleanup()
{
    return Common::TestBufferCleanup();
}

void InputTests::TestGetMouseButtonsValid()
{
    DWORD nMouseButtons = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetNumberOfConsoleMouseButtons(&nMouseButtons));

    DWORD dwButtonsExpected = (DWORD)-1;
    if (OneCoreDelay::IsGetSystemMetricsPresent())
    {
        dwButtonsExpected = (DWORD)GetSystemMetrics(SM_CMOUSEBUTTONS);
    }
    else
    {
        dwButtonsExpected = Microsoft::Console::Interactivity::OneCore::SystemConfigurationProvider::s_DefaultNumberOfMouseButtons;
    }

    VERIFY_ARE_EQUAL(dwButtonsExpected, nMouseButtons);
}

void GenerateAndWriteInputRecords(const HANDLE hConsoleInput,
                                  const UINT cRecordsToGenerate,
                                  _Out_writes_(cRecs) INPUT_RECORD* prgRecs,
                                  const DWORD cRecs,
                                  _Out_ PDWORD pdwWritten)
{
    Log::Comment(String().Format(L"Generating %d input events", cRecordsToGenerate));
    for (UINT iRecord = 0; iRecord < cRecs; iRecord++)
    {
        prgRecs[iRecord].EventType = KEY_EVENT;
        prgRecs[iRecord].Event.KeyEvent.bKeyDown = FALSE;
        prgRecs[iRecord].Event.KeyEvent.wRepeatCount = 1;
        prgRecs[iRecord].Event.KeyEvent.wVirtualKeyCode = ('A' + (WORD)iRecord);
    }

    Log::Comment(L"Writing events");
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hConsoleInput, prgRecs, cRecs, pdwWritten));
    VERIFY_ARE_EQUAL(*pdwWritten,
                     cRecs,
                     L"verify number written");
}

void InputTests::TestInputScenario()
{
    Log::Comment(L"Get input handle");
    HANDLE hConsoleInput = GetStdInputHandle();

    DWORD nWrittenEvents = (DWORD)-1;
    INPUT_RECORD rgInputRecords[NUMBER_OF_SCENARIO_INPUTS] = { 0 };
    GenerateAndWriteInputRecords(hConsoleInput, NUMBER_OF_SCENARIO_INPUTS, rgInputRecords, ARRAYSIZE(rgInputRecords), &nWrittenEvents);

    VerifyNumberOfInputRecords(hConsoleInput, ARRAYSIZE(rgInputRecords));

    Log::Comment(L"Peeking events");
    INPUT_RECORD rgPeekedRecords[NUMBER_OF_SCENARIO_INPUTS] = { 0 };
    DWORD nPeekedEvents = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(PeekConsoleInput(hConsoleInput, rgPeekedRecords, ARRAYSIZE(rgPeekedRecords), &nPeekedEvents));
    VERIFY_ARE_EQUAL(nPeekedEvents, nWrittenEvents, L"We should be able to peek at all of the records we've written");
    for (UINT iPeekedRecord = 0; iPeekedRecord < nPeekedEvents; iPeekedRecord++)
    {
        VERIFY_ARE_EQUAL(rgPeekedRecords[iPeekedRecord],
                         rgInputRecords[iPeekedRecord],
                         L"make sure our peeked records match what we input");
    }

    // read inputs 3 at a time until we've read them all. since the number we're batching by doesn't match the number of
    // total events, we need to account for the last incomplete read we'll perform.
    const UINT cIterations = (NUMBER_OF_SCENARIO_INPUTS / READ_BATCH) + ((NUMBER_OF_SCENARIO_INPUTS % READ_BATCH > 0) ? 1 : 0);
    for (UINT iIteration = 0; iIteration < cIterations; iIteration++)
    {
        const bool fIsLastIteration = (iIteration + 1) > (NUMBER_OF_SCENARIO_INPUTS / READ_BATCH);
        Log::Comment(String().Format(L"Reading inputs (iteration %d/%d)%s",
                                     iIteration + 1,
                                     cIterations,
                                     fIsLastIteration ? L" (last one)" : L""));

        INPUT_RECORD rgReadRecords[READ_BATCH] = { 0 };
        DWORD nReadEvents = (DWORD)-1;
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(hConsoleInput, rgReadRecords, ARRAYSIZE(rgReadRecords), &nReadEvents));

        DWORD dwExpectedEventsRead = READ_BATCH;
        if (fIsLastIteration)
        {
            // on the last iteration, we'll have an incomplete read. account for it here.
            dwExpectedEventsRead = NUMBER_OF_SCENARIO_INPUTS % READ_BATCH;
        }

        VERIFY_ARE_EQUAL(nReadEvents, dwExpectedEventsRead);
        for (UINT iReadRecord = 0; iReadRecord < nReadEvents; iReadRecord++)
        {
            const UINT iInputRecord = iReadRecord + (iIteration * READ_BATCH);
            VERIFY_ARE_EQUAL(rgReadRecords[iReadRecord],
                             rgInputRecords[iInputRecord],
                             String().Format(L"verify record %d", iInputRecord));
        }

        DWORD nInputEventsAfterRead = (DWORD)-1;
        VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(hConsoleInput, &nInputEventsAfterRead));

        DWORD dwExpectedEventsAfterRead = (NUMBER_OF_SCENARIO_INPUTS - (READ_BATCH * (iIteration + 1)));
        if (fIsLastIteration)
        {
            dwExpectedEventsAfterRead = 0;
        }
        VERIFY_ARE_EQUAL(dwExpectedEventsAfterRead,
                         nInputEventsAfterRead,
                         L"verify number of remaining inputs");
    }
}

void InputTests::TestFlushValid()
{
    Log::Comment(L"Get input handle");
    HANDLE hConsoleInput = GetStdInputHandle();

    DWORD nWrittenEvents = (DWORD)-1;
    INPUT_RECORD rgInputRecords[NUMBER_OF_SCENARIO_INPUTS] = { 0 };
    GenerateAndWriteInputRecords(hConsoleInput, NUMBER_OF_SCENARIO_INPUTS, rgInputRecords, ARRAYSIZE(rgInputRecords), &nWrittenEvents);

    VerifyNumberOfInputRecords(hConsoleInput, ARRAYSIZE(rgInputRecords));

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hConsoleInput));

    VerifyNumberOfInputRecords(hConsoleInput, 0);
}

void InputTests::TestFlushInvalid()
{
    // NOTE: FlushConsoleInputBuffer(nullptr) crashes, so we don't verify that here.
    VERIFY_WIN32_BOOL_FAILED(FlushConsoleInputBuffer(INVALID_HANDLE_VALUE));
}

void InputTests::TestPeekConsoleInvalid()
{
    DWORD nPeeked = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(PeekConsoleInput(INVALID_HANDLE_VALUE, nullptr, 0, &nPeeked)); // NOTE: nPeeked is required
    VERIFY_ARE_EQUAL(nPeeked, (DWORD)0);

    HANDLE hConsoleInput = GetStdInputHandle();

    nPeeked = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(PeekConsoleInput(hConsoleInput, nullptr, 5, &nPeeked));
    VERIFY_ARE_EQUAL(nPeeked, (DWORD)0);

    DWORD nWritten = (DWORD)-1;
    INPUT_RECORD ir = { 0 };
    GenerateAndWriteInputRecords(hConsoleInput, 1, &ir, 1, &nWritten);

    VerifyNumberOfInputRecords(hConsoleInput, 1);

    nPeeked = (DWORD)-1;
    INPUT_RECORD irPeeked = { 0 };
    VERIFY_WIN32_BOOL_SUCCEEDED(PeekConsoleInput(hConsoleInput, &irPeeked, 0, &nPeeked));
    VERIFY_ARE_EQUAL(nPeeked, (DWORD)0, L"Verify that an empty array doesn't cause peeks to get written");

    VerifyNumberOfInputRecords(hConsoleInput, 1);

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hConsoleInput));
}

void InputTests::TestReadConsoleInvalid()
{
    DWORD nRead = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(ReadConsoleInput(nullptr, nullptr, 0, &nRead));
    VERIFY_ARE_EQUAL(nRead, (DWORD)0);

    nRead = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(ReadConsoleInput(INVALID_HANDLE_VALUE, nullptr, 0, &nRead));
    VERIFY_ARE_EQUAL(nRead, (DWORD)0);

    // NOTE: ReadConsoleInput blocks until at least one input event is read, even if the operation would result in no
    // records actually being read (e.g. valid handle, NULL lpBuffer)

    HANDLE hConsoleInput = GetStdInputHandle();

    DWORD nWritten = (DWORD)-1;
    INPUT_RECORD irWrite = { 0 };
    GenerateAndWriteInputRecords(hConsoleInput, 1, &irWrite, 1, &nWritten);
    VerifyNumberOfInputRecords(hConsoleInput, 1);

    nRead = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(hConsoleInput, nullptr, 0, &nRead));
    VERIFY_ARE_EQUAL(nRead, (DWORD)0);

    INPUT_RECORD irRead = { 0 };
    nRead = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(hConsoleInput, &irRead, 0, &nRead));
    VERIFY_ARE_EQUAL(nRead, (DWORD)0);

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hConsoleInput));
}

void InputTests::TestWriteConsoleInvalid()
{
    DWORD nWrite = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(WriteConsoleInput(nullptr, nullptr, 0, &nWrite));
    VERIFY_ARE_EQUAL(nWrite, (DWORD)0);

    // weird: WriteConsoleInput with INVALID_HANDLE_VALUE writes garbage to lpNumberOfEventsWritten, whereas
    // [Read|Peek]ConsoleInput don't. This is a legacy behavior that we don't want to change.
    nWrite = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(WriteConsoleInput(INVALID_HANDLE_VALUE, nullptr, 0, &nWrite));

    HANDLE hConsoleInput = GetStdInputHandle();

    nWrite = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hConsoleInput, nullptr, 0, &nWrite));
    VERIFY_ARE_EQUAL(nWrite, (DWORD)0);

    nWrite = (DWORD)-1;
    INPUT_RECORD irWrite = { 0 };
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hConsoleInput, &irWrite, 0, &nWrite));
    VERIFY_ARE_EQUAL(nWrite, (DWORD)0);
}

void FillInputRecordHelper(_Inout_ INPUT_RECORD* const pir, _In_ wchar_t wch, _In_ bool fIsKeyDown)
{
    pir->EventType = KEY_EVENT;
    pir->Event.KeyEvent.wRepeatCount = 1;
    pir->Event.KeyEvent.dwControlKeyState = 0;
    pir->Event.KeyEvent.bKeyDown = !!fIsKeyDown;
    pir->Event.KeyEvent.uChar.UnicodeChar = wch;

    // This only holds true for capital letters from A-Z.
    VERIFY_IS_TRUE(wch >= 'A' && wch <= 'Z');
    pir->Event.KeyEvent.wVirtualKeyCode = wch;

    pir->Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(pir->Event.KeyEvent.wVirtualKeyCode, MAPVK_VK_TO_VSC));
}

void InputTests::TestReadConsolePasswordScenario()
{
    if (!OneCoreDelay::IsPostMessageWPresent())
    {
        Log::Comment(L"Password scenario can't be checked on platform without window message queuing.");
        Log::Result(WEX::Logging::TestResults::Skipped);
        return;
    }

    // Scenario inspired by net use's password capture code.
    HANDLE const hIn = GetStdHandle(STD_INPUT_HANDLE);

    // 1. Set up our mode to be raw input (mimicking method used by "net use")
    DWORD mode = ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT;
    GetConsoleMode(hIn, &mode);

    SetConsoleMode(hIn, (~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT)) & mode);

    // 2. Flush and write some text into the input buffer (added for sake of test.)
    PCWSTR pwszExpected = L"QUE";
    DWORD const cBuffer = static_cast<DWORD>(wcslen(pwszExpected) * 2);
    wistd::unique_ptr<INPUT_RECORD[]> irBuffer = wil::make_unique_nothrow<INPUT_RECORD[]>(cBuffer);
    FillInputRecordHelper(&irBuffer.get()[0], pwszExpected[0], true);
    FillInputRecordHelper(&irBuffer.get()[1], pwszExpected[0], false);
    FillInputRecordHelper(&irBuffer.get()[2], pwszExpected[1], true);
    FillInputRecordHelper(&irBuffer.get()[3], pwszExpected[1], false);
    FillInputRecordHelper(&irBuffer.get()[4], pwszExpected[2], true);
    FillInputRecordHelper(&irBuffer.get()[5], pwszExpected[2], false);

    DWORD dwWritten;
    FlushConsoleInputBuffer(hIn);
    WriteConsoleInputW(hIn, irBuffer.get(), cBuffer, &dwWritten);

    // Press "enter" key on the window to signify the user pressing enter at the end of the password.

    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(PostMessageW(GetConsoleWindow(), WM_KEYDOWN, VK_RETURN, 0));

    // 3. Set up our read loop (mimicking password capture methodology from "net use" command.)
    size_t const buflen = (cBuffer / 2) + 1; // key down and key up will be coalesced into one.
    wistd::unique_ptr<wchar_t[]> buf = wil::make_unique_nothrow<wchar_t[]>(buflen);
    size_t len = 0;
    VERIFY_IS_NOT_NULL(buf);
    wchar_t* bufPtr = buf.get();

    while (true)
    {
        wchar_t ch;
        DWORD c;
        int err = ReadConsoleW(hIn, &ch, 1, &c, nullptr);

        if (!err || c != 1)
        {
            ch = 0xffff; // end of line
        }

        if ((ch == 0xD) || (ch == 0xffff)) // CR or end of line
        {
            break;
        }

        if (ch == 0x8) // backspace
        {
            if (bufPtr != buf.get())
            {
                bufPtr--;
                len--;
            }
        }
        else
        {
            *bufPtr = ch;
            if (len < buflen)
            {
                bufPtr++;
            }
            len++;
        }
    }

    // 4. Restore console mode and terminate string (mimics "net use" behavior)
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), mode);

    *bufPtr = L'\0'; // null terminate string
    putchar('\n');

    // 5. Verify our string got read back (added for sake of the test)
    VERIFY_ARE_EQUAL(String(pwszExpected), String(buf.get()));
    VERIFY_ARE_EQUAL(wcslen(pwszExpected), len);
}

void TestMouseWheelReadConsoleInputHelper(const UINT /*msg*/, const DWORD /*dwEventFlagsExpected*/, const DWORD /*dwConsoleMode*/)
{
    if (!OneCoreDelay::IsIsWindowPresent())
    {
        Log::Comment(L"Mouse wheel with respect to a window can't be checked on platform without classic window message queuing.");
        Log::Result(WEX::Logging::TestResults::Skipped);
        return;
    }

    Log::Comment(L"This test is flaky. Fix me in GH#4494");
    Log::Result(WEX::Logging::TestResults::Skipped);
    return;

    //HWND const hwnd = GetConsoleWindow();
    //VERIFY_IS_TRUE(!!IsWindow(hwnd), L"Get console window handle to inject wheel messages.");

    //HANDLE const hConsoleInput = GetStdInputHandle();
    //VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hConsoleInput, dwConsoleMode), L"Apply the requested console mode");

    //// We don't generate mouse console event in QuickEditMode or if MouseInput is not enabled
    //DWORD dwExpectedEvents = 1;
    //if (dwConsoleMode & ENABLE_QUICK_EDIT_MODE || !(dwConsoleMode & ENABLE_MOUSE_INPUT))
    //{
    //    Log::Comment(L"QuickEditMode is set or MouseInput is not set, not expecting events");
    //    dwExpectedEvents = 0;
    //}

    //VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hConsoleInput), L"Flush input queue to make sure no one else is in the way.");

    //// WM_MOUSEWHEEL params
    //// https://msdn.microsoft.com/en-us/library/windows/desktop/ms645617(v=vs.85).aspx

    //// WPARAM is HIWORD the wheel delta and LOWORD the keystate (keys pressed with it)
    //// We want no keys pressed in the loword (0) and we want one tick of the wheel in the high word.
    //WPARAM wParam = 0;
    //short sKeyState = 0;
    //short sWheelDelta = -WHEEL_DELTA; // scroll down is negative, up is positive.
    //// we only use the lower 32-bits (in case of 64-bit system)
    //wParam = ((sWheelDelta << 16) | sKeyState) & 0xFFFFFFFF;

    //// LPARAM is positioning information. We don't care so we'll leave it 0x0
    //LPARAM lParam = 0;

    //Log::Comment(L"Send scroll down message into console window queue.");
    //SendMessageW(hwnd, msg, wParam, lParam);

    //Sleep(250); // give message time to sink in

    //DWORD dwAvailable = 0;
    //VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(hConsoleInput, &dwAvailable), L"Retrieve number of events in queue.");
    //VERIFY_ARE_EQUAL(dwExpectedEvents, dwAvailable, NoThrowString().Format(L"We expected %i event from our scroll message.", dwExpectedEvents));

    //INPUT_RECORD ir;
    //DWORD dwRead = 0;
    //if (dwExpectedEvents == 1)
    //{
    //    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInputW(hConsoleInput, &ir, 1, &dwRead), L"Read the event out.");
    //    VERIFY_ARE_EQUAL(1u, dwRead);

    //    Log::Comment(L"Verify the event is what we expected. We only verify the fields relevant to this test.");
    //    VERIFY_ARE_EQUAL(MOUSE_EVENT, ir.EventType);
    //    // hard cast OK. only using lower 32-bits (see above)
    //    VERIFY_ARE_EQUAL((DWORD)wParam, ir.Event.MouseEvent.dwButtonState);
    //    // Don't care about ctrl key state. Can be messed with by caps lock/numlock state. Not checking this.
    //    VERIFY_ARE_EQUAL(dwEventFlagsExpected, ir.Event.MouseEvent.dwEventFlags);
    //    // Don't care about mouse position for ensuring scroll message went through.
    //}
}

void InputTests::TestMouseWheelReadConsoleMouseInput()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEWHEEL, MOUSE_WHEELED, dwInputMode);
}

void InputTests::TestMouseHorizWheelReadConsoleMouseInput()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEHWHEEL, MOUSE_HWHEELED, dwInputMode);
}

void InputTests::TestMouseWheelReadConsoleNoMouseInput()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_EXTENDED_FLAGS;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEWHEEL, MOUSE_WHEELED, dwInputMode);
}

void InputTests::TestMouseHorizWheelReadConsoleNoMouseInput()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_EXTENDED_FLAGS;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEHWHEEL, MOUSE_HWHEELED, dwInputMode);
}

void InputTests::TestMouseWheelReadConsoleInputQuickEdit()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEWHEEL, MOUSE_WHEELED, dwInputMode);
}

void InputTests::TestMouseHorizWheelReadConsoleInputQuickEdit()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEHWHEEL, MOUSE_HWHEELED, dwInputMode);
}

void InputTests::TestReadWaitOnHandle()
{
    HANDLE const hIn = GetStdInputHandle();
    VERIFY_IS_NOT_NULL(hIn, L"Check input handle is not null.");

    // Set up events and background thread to wait.
    bool fAbortWait = false;

    // this will be signaled when we want the thread to start waiting on the input handle
    // It is an auto-reset.
    wil::unique_event doWait;
    doWait.create();

    // the thread will signal this when it is done waiting on the input handle.
    // It is an auto-reset.
    wil::unique_event doneWaiting;
    doneWaiting.create();

    std::thread bgThread([&] {
        while (!fAbortWait)
        {
            doWait.wait();

            if (fAbortWait)
            {
                break;
            }

            HANDLE waits[2];
            waits[0] = doWait.get();
            waits[1] = hIn;
            WaitForMultipleObjects(2, waits, FALSE, INFINITE);

            if (fAbortWait)
            {
                break;
            }

            doneWaiting.SetEvent();
        }
    });

    auto onExit = wil::scope_exit([&] {
        Log::Comment(L"Tell our background thread to abort waiting, signal it, then wait for it to exit before we finish the test.");
        fAbortWait = true;
        doWait.SetEvent();
        bgThread.join();
    });

    Log::Comment(L"Test 1: Waiting for text to be appended to the buffer.");
    // Empty the buffer and tell the thread to start waiting
    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn), L"Ensure input buffer is empty.");
    doWait.SetEvent();

    // Send some input into the console.
    INPUT_RECORD ir;
    ir.EventType = MOUSE_EVENT;
    ir.Event.MouseEvent.dwMousePosition.X = 1;
    ir.Event.MouseEvent.dwMousePosition.Y = 1;
    ir.Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED;
    ir.Event.MouseEvent.dwControlKeyState = NUMLOCK_ON;
    ir.Event.MouseEvent.dwEventFlags = 0;

    DWORD dwWritten = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputW(hIn, &ir, 1, &dwWritten), L"Inject input event into queue.");
    VERIFY_ARE_EQUAL(1u, dwWritten, L"Ensure 1 event was written.");

    VERIFY_IS_TRUE(doneWaiting.wait(5000), L"The input handle should have been signaled on our background thread within our 5 second timeout.");

    Log::Comment(L"Test 2: Trigger a VT response so the buffer will be prepended (things inserted at the front).");

    HANDLE const hOut = GetStdOutputHandle();
    DWORD dwMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(hOut, &dwMode), L"Get existing console mode.");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING), L"Ensure VT mode is on.");

    // Empty the buffer and tell the thread to start waiting
    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn), L"Ensure input buffer is empty.");
    doWait.SetEvent();

    // Send a VT command that will trigger a response.
    PCWSTR pwszDeviceAttributeRequest = L"\x1b[c";
    DWORD cchDeviceAttributeRequest = static_cast<DWORD>(wcslen(pwszDeviceAttributeRequest));
    dwWritten = 0;
    WriteConsoleW(hOut, pwszDeviceAttributeRequest, cchDeviceAttributeRequest, &dwWritten, nullptr);
    VERIFY_ARE_EQUAL(cchDeviceAttributeRequest, dwWritten, L"Verify string was written");

    VERIFY_IS_TRUE(doneWaiting.wait(5000), L"The input handle should have been signaled on our background thread within our 5 second timeout.");
}

void InputTests::TestVtInputGeneration()
{
    Log::Comment(L"Get input handle");
    HANDLE hIn = GetStdInputHandle();

    DWORD dwMode;
    GetConsoleMode(hIn, &dwMode);

    DWORD dwWritten = (DWORD)-1;
    DWORD dwRead = (DWORD)-1;
    INPUT_RECORD rgInputRecords[64] = { 0 };

    Log::Comment(L"First make sure that an arrow keydown is not translated in not-VT mode");

    dwMode = WI_ClearFlag(dwMode, ENABLE_VIRTUAL_TERMINAL_INPUT);
    SetConsoleMode(hIn, dwMode);
    GetConsoleMode(hIn, &dwMode);
    VERIFY_IS_FALSE(WI_IsFlagSet(dwMode, ENABLE_VIRTUAL_TERMINAL_INPUT));

    rgInputRecords[0].EventType = KEY_EVENT;
    rgInputRecords[0].Event.KeyEvent.bKeyDown = TRUE;
    rgInputRecords[0].Event.KeyEvent.wRepeatCount = 1;
    rgInputRecords[0].Event.KeyEvent.wVirtualKeyCode = VK_UP;

    Log::Comment(L"Writing events");
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hIn, rgInputRecords, 1, &dwWritten));
    VERIFY_ARE_EQUAL(dwWritten, (DWORD)1);

    Log::Comment(L"Reading events");
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(hIn, rgInputRecords, ARRAYSIZE(rgInputRecords), &dwRead));
    VERIFY_ARE_EQUAL(dwRead, (DWORD)1);
    VERIFY_ARE_EQUAL(rgInputRecords[0].EventType, KEY_EVENT);
    VERIFY_ARE_EQUAL(rgInputRecords[0].Event.KeyEvent.bKeyDown, TRUE);
    VERIFY_ARE_EQUAL(rgInputRecords[0].Event.KeyEvent.wVirtualKeyCode, VK_UP);

    Log::Comment(L"Now, enable VT Input and make sure that a vt sequence comes out the other side.");

    dwMode = WI_SetFlag(dwMode, ENABLE_VIRTUAL_TERMINAL_INPUT);
    SetConsoleMode(hIn, dwMode);
    GetConsoleMode(hIn, &dwMode);
    VERIFY_IS_TRUE(WI_IsFlagSet(dwMode, ENABLE_VIRTUAL_TERMINAL_INPUT));

    Log::Comment(L"Flushing");
    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn));

    rgInputRecords[0].EventType = KEY_EVENT;
    rgInputRecords[0].Event.KeyEvent.bKeyDown = TRUE;
    rgInputRecords[0].Event.KeyEvent.wRepeatCount = 1;
    rgInputRecords[0].Event.KeyEvent.wVirtualKeyCode = VK_UP;

    Log::Comment(L"Writing events");
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hIn, rgInputRecords, 1, &dwWritten));
    VERIFY_ARE_EQUAL(dwWritten, (DWORD)1);

    Log::Comment(L"Reading events");
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(hIn, rgInputRecords, ARRAYSIZE(rgInputRecords), &dwRead));
    VERIFY_ARE_EQUAL(dwRead, (DWORD)3);
    VERIFY_ARE_EQUAL(rgInputRecords[0].EventType, KEY_EVENT);
    VERIFY_ARE_EQUAL(rgInputRecords[0].Event.KeyEvent.bKeyDown, TRUE);
    VERIFY_ARE_EQUAL(rgInputRecords[0].Event.KeyEvent.wVirtualKeyCode, 0);
    VERIFY_ARE_EQUAL(rgInputRecords[0].Event.KeyEvent.uChar.UnicodeChar, L'\x1b');

    VERIFY_ARE_EQUAL(rgInputRecords[1].EventType, KEY_EVENT);
    VERIFY_ARE_EQUAL(rgInputRecords[1].Event.KeyEvent.bKeyDown, TRUE);
    VERIFY_ARE_EQUAL(rgInputRecords[1].Event.KeyEvent.wVirtualKeyCode, 0);
    VERIFY_ARE_EQUAL(rgInputRecords[1].Event.KeyEvent.uChar.UnicodeChar, L'[');

    VERIFY_ARE_EQUAL(rgInputRecords[2].EventType, KEY_EVENT);
    VERIFY_ARE_EQUAL(rgInputRecords[2].Event.KeyEvent.bKeyDown, TRUE);
    VERIFY_ARE_EQUAL(rgInputRecords[2].Event.KeyEvent.wVirtualKeyCode, 0);
    VERIFY_ARE_EQUAL(rgInputRecords[2].Event.KeyEvent.uChar.UnicodeChar, L'A');
}

void InputTests::RawReadUnpacksCoalescedInputRecords()
{
    DWORD mode = 0;
    HANDLE hIn = GetStdInputHandle();
    const wchar_t writeWch = L'a';
    const auto repeatCount = 5;

    // turn on raw mode
    GetConsoleMode(hIn, &mode);
    WI_ClearFlag(mode, ENABLE_LINE_INPUT);
    SetConsoleMode(hIn, mode);

    // flush input queue before attempting to add new events and check
    // in case any are leftover from previous tests
    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn));

    INPUT_RECORD record;
    record.EventType = KEY_EVENT;
    record.Event.KeyEvent.bKeyDown = TRUE;
    record.Event.KeyEvent.wRepeatCount = repeatCount;
    record.Event.KeyEvent.wVirtualKeyCode = writeWch;
    record.Event.KeyEvent.uChar.UnicodeChar = writeWch;

    // write an event with a repeat count
    DWORD writtenAmount = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hIn, &record, 1, &writtenAmount));
    VERIFY_ARE_EQUAL(writtenAmount, static_cast<DWORD>(1));

    // stream read the events out one at a time
    DWORD eventCount = 0;
    for (int i = 0; i < repeatCount; ++i)
    {
        eventCount = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(hIn, &eventCount));
        VERIFY_IS_TRUE(eventCount > 0);

        wchar_t wch;
        DWORD readAmount;
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsole(hIn, &wch, 1, &readAmount, NULL));
        VERIFY_ARE_EQUAL(readAmount, static_cast<DWORD>(1));
        VERIFY_ARE_EQUAL(wch, writeWch);
    }

    // input buffer should now be empty
    eventCount = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(hIn, &eventCount));
    VERIFY_ARE_EQUAL(eventCount, static_cast<DWORD>(0));
}


static std::vector<INPUT_RECORD> _stringToInputs(std::wstring_view wstr)
{
    std::vector<INPUT_RECORD> result;
    for (const auto& wch : wstr)
    {
        INPUT_RECORD ir = { 0 };
        ir.EventType = KEY_EVENT;
        ir.Event.KeyEvent.bKeyDown = TRUE;
        ir.Event.KeyEvent.dwControlKeyState = 0;
        ir.Event.KeyEvent.uChar.UnicodeChar = wch;
        ir.Event.KeyEvent.wRepeatCount = 1;
        ir.Event.KeyEvent.wVirtualKeyCode = VkKeyScanW(wch);
        ir.Event.KeyEvent.wVirtualScanCode = gsl::narrow<WORD>(MapVirtualKeyW(ir.Event.KeyEvent.wVirtualKeyCode, MAPVK_VK_TO_VSC));

        result.emplace_back(ir);

        ir.Event.KeyEvent.bKeyDown = FALSE;

        result.emplace_back(ir);
    }
    return result;
}

static HRESULT _sendStringToInput(HANDLE in, std::wstring_view wstr)
{
    auto records = _stringToInputs(wstr);
    DWORD written;
    RETURN_IF_WIN32_BOOL_FALSE(WriteConsoleInputW(in, records.data(), gsl::narrow<DWORD>(records.size()), &written));
    return S_OK;
}

// Routine Description:
// - Reads data from the standard input with a 5 second timeout
// Arguments:
// - in - The standard input handle
// - buf - The buffer to use. On in, this is the max size we'll read. On out, it's resized to fit.
// - async - Whether to read async, default to true. Reading async will put a 5 second timeout on the read.
// Return Value:
// - S_OK or an error from ReadConsole/threading timeout.
static HRESULT _readStringFromInput(HANDLE in, std::string& buf, bool async = true)
{
    DWORD read = 0;

    if (async)
    {
        auto tryRead = std::async(std::launch::async, [&] {
            return _readStringFromInput(in, buf, false); // just re-enter ourselves on the other thread as sync.
        });

        if (std::future_status::ready != tryRead.wait_for(std::chrono::seconds{ 5 }))
        {
            // Shove something into the input to unstick it then fail.
            _sendStringToInput(in, L"a\r\n");
            RETURN_NTSTATUS(STATUS_TIMEOUT);

            // If somehow this still isn't enough to unstick the thread, be sure to set
            // the whole test timeout is 1 min in the parameters/metadata at the top.
        }
        else
        {
            return tryRead.get();
        }
    }
    else
    {
        RETURN_IF_WIN32_BOOL_FALSE(ReadConsoleA(in, buf.data(), gsl::narrow<DWORD>(buf.size()), &read, nullptr));
        // If we successfully read, then resize to fit the buffer.
        buf.resize(read);
        return S_OK;
    }
}

static HRESULT _readStringFromInputDirect(HANDLE in, std::string& buf, bool async = true)
{
    if (async)
    {
        auto tryRead = std::async(std::launch::async, [&] {
            return _readStringFromInputDirect(in, buf, false); // just re-enter ourselves on the other thread as sync.
        });

        if (std::future_status::ready != tryRead.wait_for(std::chrono::seconds{ 5 }))
        {
            // Shove something into the input to unstick it then fail.
            _sendStringToInput(in, L"a\r\n");
            RETURN_NTSTATUS(STATUS_TIMEOUT);

            // If somehow this still isn't enough to unstick the thread, be sure to set
            // the whole test timeout is 1 min in the parameters/metadata at the top.
        }
        else
        {
            return tryRead.get();
        }
    }
    else
    {
        const auto originalSize = buf.size();
        buf.clear();

        std::vector<INPUT_RECORD> ir;

        DWORD read = 0;
        do
        {
            ir.clear();
            ir.resize(originalSize - buf.size());

            RETURN_IF_WIN32_BOOL_FALSE(ReadConsoleInputA(in, ir.data(), gsl::narrow_cast<DWORD>(ir.size()), &read));

            for (const auto& r : ir)
            {
                if (r.EventType == KEY_EVENT)
                {
                    if (!r.Event.KeyEvent.bKeyDown)
                    {
                        buf.push_back(r.Event.KeyEvent.uChar.AsciiChar);
                    }
                }
            }
            ir.clear();
        } while (originalSize > buf.size());

        return S_OK;
    }
}

void InputTests::TestCookedAliasProcessing()
{
    const auto in = GetStdInputHandle();

    DWORD originalInMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));

    DWORD originalCodepage = GetConsoleCP();

    auto restoreInModeOnExit = wil::scope_exit([&] {
        SetConsoleMode(in, originalInMode);
        SetConsoleCP(originalCodepage);
    });

    const DWORD testInMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, testInMode));

    auto modulePath = wil::GetModuleFileNameW<std::wstring>(nullptr);
    std::filesystem::path path{ modulePath };
    auto fileName = path.filename();
    auto exeName = fileName.wstring();

    VERIFY_WIN32_BOOL_SUCCEEDED(AddConsoleAliasW(L"foo", L"echo bar$Techo baz$Techo bam", exeName.data()));

    std::wstring commandWritten = L"foo\r\n";
    std::queue<std::string> commandExpected;
    commandExpected.push("echo bar\r");
    commandExpected.push("echo baz\r");
    commandExpected.push("echo bam\r");

    VERIFY_SUCCEEDED(_sendStringToInput(in, commandWritten));

    std::string buf;

    while (!commandExpected.empty())
    {
        buf.resize(500);

        VERIFY_SUCCEEDED(_readStringFromInput(in, buf));

        auto actual = buf;

        auto expected = commandExpected.front();
        commandExpected.pop();

        VERIFY_ARE_EQUAL(expected, actual);
    }
}

void InputTests::TestCookedTextEntry()
{
    const auto in = GetStdInputHandle();

    DWORD originalInMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));

    DWORD originalCodepage = GetConsoleCP();

    auto restoreInModeOnExit = wil::scope_exit([&] {
        SetConsoleMode(in, originalInMode);
        SetConsoleCP(originalCodepage);
    });

    const DWORD testInMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, testInMode));

    std::wstring commandWritten = L"foo\r\n";
    std::queue<std::string> commandExpected;
    commandExpected.push("foo\r\n");

    VERIFY_SUCCEEDED(_sendStringToInput(in, commandWritten));

    std::string buf;

    while (!commandExpected.empty())
    {
        buf.resize(500);

        VERIFY_SUCCEEDED(_readStringFromInput(in, buf));

        auto actual = buf;

        auto expected = commandExpected.front();
        commandExpected.pop();

        VERIFY_ARE_EQUAL(expected, actual);
    }
}

// tests todo:
// - test alpha in/out in 437/932 permutations
// - test leftover leadbyte/trailbyte when buffer too small
void InputTests::TestCookedAlphaPermutations()
{
    DWORD inputcp, outputcp, inputmode, outputmode;
    String font;

    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"inputcp", inputcp), L"Get input cp");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"outputcp", outputcp), L"Get output cp");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"inputmode", inputmode), L"Get input mode");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"outputmode", outputmode), L"Get output mode");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"font", font), L"Get font");

    std::wstring wstrFont{ font };
    if (wstrFont == L"MS Gothic")
    {
        // MS Gothic... but in full width characters and the katakana representation...
        // MS GOSHIKKU romanized...
        wstrFont = L"\xff2d\xff33\x0020\x30b4\x30b7\x30c3\x30af";
    }

    const auto in = GetStdInputHandle();
    const auto out = GetStdOutputHandle();

    Log::Comment(L"Backup original modes and codepages and font.");

    DWORD originalInMode, originalOutMode, originalInputCP, originalOutputCP;
    CONSOLE_FONT_INFOEX originalFont = { 0 };
    originalFont.cbSize = sizeof(originalFont);

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(out, &originalOutMode));
    originalInputCP = GetConsoleCP();
    originalOutputCP = GetConsoleOutputCP();
    VERIFY_WIN32_BOOL_SUCCEEDED(GetCurrentConsoleFontEx(out, FALSE, &originalFont));

    auto restoreModesOnExit = wil::scope_exit([&] {
        SetConsoleMode(in, originalInMode);
        SetConsoleMode(out, originalOutMode);
        SetConsoleCP(originalInputCP);
        SetConsoleOutputCP(originalOutputCP);
        SetCurrentConsoleFontEx(out, FALSE, &originalFont);
    });

    Log::Comment(L"Apply our modes and codepages and font.");

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, inputmode));
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(out, outputmode));
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(inputcp));
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleOutputCP(outputcp));

    auto ourFont = originalFont;
    wmemcpy_s(ourFont.FaceName, ARRAYSIZE(ourFont.FaceName), wstrFont.data(), wstrFont.size());

    VERIFY_WIN32_BOOL_SUCCEEDED(SetCurrentConsoleFontEx(out, FALSE, &ourFont));

    const wchar_t alpha = L'\u03b1';
    const std::string alpha437 = "\xe0";
    const std::string alpha932 = "\x83\xbf";

    std::string expected = inputcp == 932 ? alpha932 : alpha437;

    std::wstring sendInput;
    sendInput.append(&alpha, 1);

    // If we're in line input, we have to send a newline and we'll get one back.
    if (WI_IsFlagSet(inputmode, ENABLE_LINE_INPUT))
    {
        expected.append("\r\n");
        sendInput.append(L"\r\n");
    }

    Log::Comment(L"send the string");
    VERIFY_SUCCEEDED(_sendStringToInput(in, sendInput));

    Log::Comment(L"receive the string");
    std::string recvInput;
    recvInput.resize(500); // excessively big

    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));

    // corruption magic
    // In MS Gothic, alpha is full width (2 columns)
    // In Consolas, alpha is half width (1 column)
    // Alpha itself is an ambiguous character, meaning the console finds the width
    // by asking the font.
    // Unfortunately, there's some code mixed up in the cooked read for a long time where
    // the width is used as a predictor of how many bytes it will consume.
    // In this specific combination of using a font where the ambiguous alpha is half width,
    // the output code page doesn't support double bytes, and the input code page does...
    // The result is stomped with a null as the conversion fails thinking it doesn't have enough space.
    if (wstrFont == L"Consolas" && inputcp == 932 && outputcp == 437)
    {
        VERIFY_IS_GREATER_THAN_OR_EQUAL(recvInput.size(), 1);

        VERIFY_ARE_EQUAL('\x00', recvInput[0]);

        if (WI_IsFlagSet(inputmode, ENABLE_LINE_INPUT))
        {
            VERIFY_IS_GREATER_THAN_OR_EQUAL(recvInput.size(), 3);
            VERIFY_ARE_EQUAL('\r', recvInput[1]);
            VERIFY_ARE_EQUAL('\n', recvInput[2]);
        }
    }
    // end corruption magic
    else
    {
        VERIFY_ARE_EQUAL(expected, recvInput);
    }
}

// TODO tests:
// - leaving behind a lead/trail byte
// - leaving behind a lead/trail byte and having more data
// -- doing it in a loop/continuously.
// - read it char by char
// - ensure leftover bytes are lost when read off a different handle?!

void InputTests::TestCookedReadCharByChar()
{
    const auto in = GetStdInputHandle();

    DWORD originalInMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));

    DWORD originalCodepage = GetConsoleCP();

    auto restoreInModeOnExit = wil::scope_exit([&] {
        SetConsoleMode(in, originalInMode);
        SetConsoleCP(originalCodepage);
    });

    const DWORD testInMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, testInMode));

    Log::Comment(L"Set the codepage to Japanese");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(932));

    Log::Comment(L"Write something into the read queue.");

    // Greek letters, lowercase...
    const std::array<std::wstring, 4> wide = {
        L"\u03b1", // alpha
        L"\u03b2", // beta
        // no gamma because it doesn't translate to 437
        L"\u03b4", // delta
        L"\u03b5" //epsilon
    };

    const std::array<std::string, 4> char932 = {
        "\x83\xbf",
        "\x83\xc0",
        "\x83\xc2",
        "\x83\xc3"
    };

    const std::string crlf = "\r\n";

    std::wstring sendInput;
    sendInput.append(wide[0]);
    sendInput.append(wide[1]);
    sendInput.append(wide[2]);
    sendInput.append(wide[3]);
    sendInput.append(L"\r\n"); // send a newline to finish the line since we're in ENABLE_LINE_INPUT mode

    Log::Comment(L"send the string");
    VERIFY_SUCCEEDED(_sendStringToInput(in, sendInput));

    Log::Comment(L"Read byte by byte, should leave trailing each time.");
    std::string expectedInput;
    expectedInput = char932[0][0];
       
    // this is an artifact of resizing our string to the `lpNumberOfCharsRead`
    // which can be longer than the buffer we gave. `ReadConsoleA` appears to
    // do this either to signal there are more or as a mistake that was never
    // matched up on API review.
    expectedInput.append(1, '\0');

    std::string recvInput;
    recvInput.resize(1); // two bytes of first alpha and then a lead byte of the second one.
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    // TODO: CHv1 completely loses the trailing byte.
    
    expectedInput = char932[1][0];
    expectedInput.append(1, '\0');
    recvInput.clear();
    recvInput.resize(1);
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    // TODO: CHv1 completely loses the trailing byte.
    
    expectedInput = char932[2][0];
    expectedInput.append(1, '\0');
    recvInput.clear();
    recvInput.resize(1);
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    // TODO: CHv1 completely loses the trailing byte.

    expectedInput = char932[3][0];
    expectedInput.append(1, '\0');
    recvInput.clear();
    recvInput.resize(1);
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    // TODO: CHv1 completely loses the trailing byte.

    expectedInput = crlf[0];
    recvInput.clear();
    recvInput.resize(1);
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    expectedInput = crlf[1];
    recvInput.clear();
    recvInput.resize(1);
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);
}

void InputTests::TestDirectReadCharByChar()
{
    const auto in = GetStdInputHandle();

    DWORD originalInMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));

    DWORD originalCodepage = GetConsoleCP();

    auto restoreInModeOnExit = wil::scope_exit([&] {
        SetConsoleMode(in, originalInMode);
        SetConsoleCP(originalCodepage);
    });

    const DWORD testInMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, testInMode));

    Log::Comment(L"Set the codepage to Japanese");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(932));

    Log::Comment(L"Write something into the read queue.");

    // Greek letters, lowercase...
    const std::array<std::wstring, 4> wide = {
        L"\u03b1", // alpha
        L"\u03b2", // beta
        // no gamma because it doesn't translate to 437
        L"\u03b4", // delta
        L"\u03b5" //epsilon
    };

    const std::array<std::string, 4> char932 = {
        "\x83\xbf",
        "\x83\xc0",
        "\x83\xc2",
        "\x83\xc3"
    };

    const std::string crlf = "\r\n";

    std::wstring sendInput;
    sendInput.append(wide[0]);
    sendInput.append(wide[1]);
    sendInput.append(wide[2]);
    sendInput.append(wide[3]);
    sendInput.append(L"\r\n"); // send a newline to finish the line since we're in ENABLE_LINE_INPUT mode

    Log::Comment(L"send the string");
    VERIFY_SUCCEEDED(_sendStringToInput(in, sendInput));

    Log::Comment(L"Read byte by byte, should leave trailing each time.");
    std::string expectedInput;
    expectedInput = char932[0][0];

    std::string recvInput;
    recvInput.resize(1); // only gets the leading byte
    VERIFY_SUCCEEDED(_readStringFromInputDirect(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    // TODO v1, loses the trailing byte

    expectedInput = char932[1][0];
    recvInput.clear();
    recvInput.resize(1);
    VERIFY_SUCCEEDED(_readStringFromInputDirect(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    // TODO v1, loses the trailing byte

    expectedInput = char932[2][0];
    recvInput.clear();
    recvInput.resize(1);
    VERIFY_SUCCEEDED(_readStringFromInputDirect(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    // TODO v1, loses the trailing byte

    expectedInput = char932[3][0];
    recvInput.clear();
    recvInput.resize(1);
    VERIFY_SUCCEEDED(_readStringFromInputDirect(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    // TODO v1, loses the trailing byte

    expectedInput = crlf[0];
    recvInput.clear();
    recvInput.resize(1);
    VERIFY_SUCCEEDED(_readStringFromInputDirect(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    expectedInput = crlf[1];
    recvInput.clear();
    recvInput.resize(1);
    VERIFY_SUCCEEDED(_readStringFromInputDirect(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);
}

void InputTests::TestCookedReadLeadTrailString()
{
    const auto in = GetStdInputHandle();

    DWORD originalInMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));

    DWORD originalCodepage = GetConsoleCP();

    auto restoreInModeOnExit = wil::scope_exit([&] {
        SetConsoleMode(in, originalInMode);
        SetConsoleCP(originalCodepage);
    });

    const DWORD testInMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, testInMode));

    Log::Comment(L"Set the codepage to Japanese");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(932));

    Log::Comment(L"Write something into the read queue.");

    // Greek letters, lowercase...
    const std::array<std::wstring, 4> wide = {
        L"\u03b1", // alpha
        L"\u03b2", // beta
        // no gamma because it doesn't translate to 437
        L"\u03b4", // delta
        L"\u03b5" //epsilon
    };

    const std::array<std::string, 4> char932 = {
        "\x83\xbf",
        "\x83\xc0",
        "\x83\xc2",
        "\x83\xc3"
    };

    const std::string crlf = "\r\n";

    std::wstring sendInput;
    sendInput.append(wide[0]);
    sendInput.append(wide[1]);
    sendInput.append(wide[2]);
    sendInput.append(wide[3]);
    sendInput.append(L"\r\n"); // send a newline to finish the line since we're in ENABLE_LINE_INPUT mode

    Log::Comment(L"send the string");
    VERIFY_SUCCEEDED(_sendStringToInput(in, sendInput));

    Log::Comment(L"Read first byte, should leave trailing.");
    std::string expectedInput;
    expectedInput = char932[0][0];

    // this is an artifact of resizing our string to the `lpNumberOfCharsRead`
    // which can be longer than the buffer we gave. `ReadConsoleA` appears to
    // do this either to signal there are more or as a mistake that was never
    // matched up on API review.
    expectedInput.append(1, '\0');

    std::string recvInput;
    recvInput.resize(1); // two bytes of first alpha and then a lead byte of the second one.
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));

    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    // TODO: CHv1 completely loses the trailing byte.

    Log::Comment(L"Read everything else.");

    expectedInput = char932[1];
    expectedInput.append(char932[2]);
    expectedInput.append(char932[3]);
    expectedInput.append(crlf);
    recvInput.clear();
    recvInput.resize(100);
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);
}

void InputTests::TestDirectReadLeadTrailString()
{
    const auto in = GetStdInputHandle();

    DWORD originalInMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));

    DWORD originalCodepage = GetConsoleCP();

    auto restoreInModeOnExit = wil::scope_exit([&] {
        SetConsoleMode(in, originalInMode);
        SetConsoleCP(originalCodepage);
    });

    const DWORD testInMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, testInMode));

    Log::Comment(L"Set the codepage to Japanese");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(932));

    Log::Comment(L"Write something into the read queue.");

    // Greek letters, lowercase...
    const std::array<std::wstring, 4> wide = {
        L"\u03b1", // alpha
        L"\u03b2", // beta
        // no gamma because it doesn't translate to 437
        L"\u03b4", // delta
        L"\u03b5" //epsilon
    };

    const std::array<std::string, 4> char932 = {
        "\x83\xbf",
        "\x83\xc0",
        "\x83\xc2",
        "\x83\xc3"
    };

    const std::string crlf = "\r\n";

    std::wstring sendInput;
    sendInput.append(wide[0]);
    sendInput.append(wide[1]);
    sendInput.append(wide[2]);
    sendInput.append(wide[3]);
    sendInput.append(L"\r\n"); // send a newline to finish the line since we're in ENABLE_LINE_INPUT mode

    Log::Comment(L"send the string");
    VERIFY_SUCCEEDED(_sendStringToInput(in, sendInput));

    Log::Comment(L"Read first byte, should leave trailing.");
    std::string expectedInput;
    expectedInput = char932[0][0];

    std::string recvInput;
    recvInput.resize(1); // two bytes of first alpha and then a lead byte of the second one.
    VERIFY_SUCCEEDED(_readStringFromInputDirect(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    Log::Comment(L"Read everything else.");

    expectedInput = char932[0][1];
    expectedInput.append(char932[1]);
    expectedInput.append(char932[2]);
    expectedInput.append(char932[3]);
    expectedInput.append(crlf);
    recvInput.clear();
    recvInput.resize(9);
    VERIFY_SUCCEEDED(_readStringFromInputDirect(in, recvInput));
    VERIFY_ARE_EQUAL(expectedInput, recvInput);
}

void InputTests::TestCookedReadChangeCodepageInMiddle()
{
    const auto in = GetStdInputHandle();

    DWORD originalInMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));

    DWORD originalCodepage = GetConsoleCP();

    auto restoreInModeOnExit = wil::scope_exit([&] {
        SetConsoleMode(in, originalInMode);
        SetConsoleCP(originalCodepage);
    });

    const DWORD testInMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, testInMode));

    Log::Comment(L"Set the codepage to Japanese");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(932));

    Log::Comment(L"Write something into the read queue.");

    // Greek letters, lowercase...
    const std::array<std::wstring, 4> wide = {
        L"\u03b1", // alpha
        L"\u03b2", // beta
        // no gamma because it doesn't translate to 437
        L"\u03b4", // delta
        L"\u03b5" //epsilon
    };

    const std::array<std::string, 4> char437 = {
        "\xe0",
        "\xe1",
        "\xeb",
        "\xee"
    };

    const std::array<std::string, 4> char932 = {
        "\x83\xbf",
        "\x83\xc0",
        "\x83\xc2",
        "\x83\xc3"
    };

    const std::string crlf = "\r\n";

    std::wstring sendInput;
    sendInput.append(wide[0]);
    sendInput.append(wide[1]);
    sendInput.append(wide[2]);
    sendInput.append(wide[3]);
    sendInput.append(L"\r\n"); // send a newline to finish the line since we're in ENABLE_LINE_INPUT mode

    Log::Comment(L"send the string");
    VERIFY_SUCCEEDED(_sendStringToInput(in, sendInput));

    Log::Comment(L"Read only part of it including leaving behind a trailing byte.");
    std::string expectedInput;
    expectedInput = char932[0];

    // The following two only happen if you switch part way through...
    expectedInput.append(char932[1].data(), 1);
    // this is an artifact of resizing our string to the `lpNumberOfCharsRead`
    // which can be longer than the buffer we gave. `ReadConsoleA` appears to
    // do this either to signal there are more or as a mistake that was never
    // matched up on API review.
    expectedInput.append(1, '\0');

    std::string recvInput;
    recvInput.resize(3); // two bytes of first alpha and then a lead byte of the second one.
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));

    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    Log::Comment(L"Set the codepage to English");
    Log::Comment(L"Changing codepage should discard all partial bytes!");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(437));

    Log::Comment(L"Read the rest of it and validate that it was re-encoded as English");
    expectedInput.clear();
    // TODO: I believe v2 shouldn't lose this character by switching codepages.
    //expectedInput.append(char437[2]);
    expectedInput.append(char437[3]);
    expectedInput.append(crlf);

    recvInput.clear();
    recvInput.resize(490);
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));

    VERIFY_ARE_EQUAL(expectedInput, recvInput);
}

void InputTests::TestCookedReadChangeCodepageBetweenBytes()
{
    const auto in = GetStdInputHandle();

    DWORD originalInMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));

    DWORD originalCodepage = GetConsoleCP();

    auto restoreInModeOnExit = wil::scope_exit([&] {
        SetConsoleMode(in, originalInMode);
        SetConsoleCP(originalCodepage);
    });

    const DWORD testInMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, testInMode));

    Log::Comment(L"Set the codepage to Japanese");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(932));

    Log::Comment(L"Write something into the read queue.");

    // Greek letters, lowercase...
    const std::array<std::wstring, 4> wide = {
        L"\u03b1", // alpha
        L"\u03b2", // beta
        // no gamma because it doesn't translate to 437
        L"\u03b4", // delta
        L"\u03b5" //epsilon
    };

    const std::array<std::string, 4> char437 = {
        "\xe0",
        "\xe1",
        "\xeb",
        "\xee"
    };

    const std::array<std::string, 4> char932 = {
        "\x83\xbf",
        "\x83\xc0",
        "\x83\xc2",
        "\x83\xc3"
    };

    const std::string crlf = "\r\n";

    std::wstring sendInput;
    sendInput.append(wide[0]);
    sendInput.append(wide[1]);
    sendInput.append(wide[2]);
    sendInput.append(wide[3]);
    sendInput.append(L"\r\n"); // send a newline to finish the line since we're in ENABLE_LINE_INPUT mode

    Log::Comment(L"send the string");
    VERIFY_SUCCEEDED(_sendStringToInput(in, sendInput));

    Log::Comment(L"Read only part of it including leaving behind a trailing byte.");
    std::string expectedInput;
    expectedInput = char932[0];

    std::string recvInput;
    recvInput.resize(2); // two bytes of first alpha
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));

    VERIFY_ARE_EQUAL(expectedInput, recvInput);

    Log::Comment(L"Set the codepage to English");
    Log::Comment(L"Changing codepage should discard all partial bytes!");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(437));

    Log::Comment(L"Read the rest of it and validate that it was re-encoded as English");
    expectedInput.clear();
    // TODO: I believe v2 shouldn't lose this character by switching codepages.
    // expectedInput.append(char437[1]);
    expectedInput.append(char437[2]);
    expectedInput.append(char437[3]);
    expectedInput.append(crlf);

    recvInput.clear();
    recvInput.resize(490);
    VERIFY_SUCCEEDED(_readStringFromInput(in, recvInput));

    VERIFY_ARE_EQUAL(expectedInput, recvInput);
}
