#include <atomic>
#include <cstdint>

using PositionIndex = uint32_t;

constexpr PositionIndex bufferSize = 1620;

enum State {
    EMPTY,
    SET,
    HANDLED
};

template <typename T>
class Node {
    friend class BufferList<T>;
    friend class MPSCQueue<T>;
private:
    T data;
    std::atomic<State> isSet;
    Node() : isSet(State::EMPTY) {}
};

template <typename T>
class BufferList {
    friend class MPSCQueue<T>;
private:
    Node<T>* currBuffer;
    std::atomic<BufferList*> next;
    BufferList* prev;
    PositionIndex head;
    PositionIndex positionInQueue;

    BufferList(BufferList* previousBufferList, PositionIndex head, PositionIndex positionInQueue)
        : currBuffer(new Node<T>[bufferSize]), next(nullptr), prev(previousBufferList),
          head(head), positionInQueue(positionInQueue) {}

    ~BufferList() { delete[] currBuffer; }
};

template <typename T>
class MPSCQueue {
    BufferList<T>* headOfQueue;
    std::atomic<BufferList<T>*> tailOfQueue;
    std::atomic<PositionIndex> tail;
    BufferList<T>* garbageListHead;

public:
    MPSCQueue() : headOfQueue(new BufferList<T>(nullptr, 0, 1)), tailOfQueue(headOfQueue), tail(0), garbageListHead(nullptr) {}

    void enqueue(const T& data) {
        PositionIndex location = tail.fetch_add(1, std::memory_order_relaxed);
        bool isLastBuffer = true;
        BufferList<T>* tempTail = tailOfQueue.load();
        PositionIndex numberOfElements = bufferSize * tempTail->positionInQueue;

        while (location >= numberOfElements) {
            if (tempTail->next.load() == nullptr) {
                BufferList<T>* newBuffer = new BufferList<T>(tempTail, 0, tempTail->positionInQueue + 1);

                BufferList<T>* expected_null = nullptr;
                if (tempTail->next.compare_exchange_strong(expected_null, newBuffer, std::memory_order_release)) {
                    tailOfQueue.compare_exchange_strong(tempTail, newBuffer, std::memory_order_release);
                } else {
                    delete newBuffer;
                }
            }
            tempTail = tailOfQueue.load();
            numberOfElements = bufferSize * tempTail->positionInQueue;
        }

        PositionIndex prevSize = bufferSize * (tempTail->positionInQueue - 1);

        while (location < prevSize) {
            tempTail = tempTail->prev;
            prevSize = bufferSize * (tempTail->positionInQueue - 1);
            isLastBuffer = false;
        }

        Node<T>* n = &(tempTail->currBuffer[location - prevSize]);

        if (n->isSet.load() == State::EMPTY) {
            n->data = data;
            n->isSet.store(State::SET);

            if ((location - prevSize) == 1 && isLastBuffer) {
                BufferList<T>* newBuffer = new BufferList<T>(tempTail, 0, tempTail->positionInQueue + 1);

                BufferList<T>* expected_null = nullptr;
                if (!tempTail->next.compare_exchange_strong(expected_null, newBuffer, std::memory_order_release)) {
                    delete newBuffer;
                }
            }
        }
    }

    bool dequeue(T& data) {
        Node<T>* n = &(headOfQueue->currBuffer[headOfQueue->head]);
        while (n->isSet.load() == State::HANDLED)
        {
            headOfQueue->head++;
            bool res = moveToNextBuffer();
             if (!res) {
                return false;
            }
            n = &(headOfQueue->currBuffer[headOfQueue->head]);
        }
       
        if (headOfQueue == tailOfQueue.load() && headOfQueue->head == tail.load() % bufferSize) {
            return false;
        }


        if (n->isSet.load() == State::SET) {
            headOfQueue->head++;
            moveToNextBuffer();
            data = n->data;
            return true;
        }

        if (n->isSet.load() == State::EMPTY) {
            BufferList<T>* tempHeadOfQueue = headOfQueue;
            PositionIndex tempHead = headOfQueue->head;
            Node<T>* tempN = &(tempHeadOfQueue->currBuffer[tempHead]);
            bool res = Scan(tempHeadOfQueue, tempHead, tempN);
            if (!res) {
                return false;
            }
            ReScan(headOfQueue, tempHeadOfQueue, tempHead, tempN);
            data = tempN->data;
            tempN->isSet.store(State::HANDLED);
            if (tempHeadOfQueue == headOfQueue && tempHead == headOfQueue->head) {
                headOfQueue->head++;
                moveToNextBuffer();
            }
            return true;
        }
        
        return false;
    }

    bool moveToNextBuffer() {
        if (headOfQueue->head >= bufferSize) {
            if (headOfQueue == tailOfQueue.load(std::memory_order_acquire)) {
                return false;
            }
            BufferList<T>* next = headOfQueue->next.load(std::memory_order_acquire);
            if (next == nullptr) {
                return false;
            }
            BufferList<T>* g = garbageListHead;
            while (g != nullptr && g->positionInQueue < next->positionInQueue) {
                garbageListHead = g->next.load(std::memory_order_relaxed);
                delete g;
                g = garbageListHead;
            }
            delete headOfQueue;
            headOfQueue = next;
        }
        return true;
    }

    bool fold(BufferList<T>*& tempHeadOfQueue, PositionIndex& tempHead, bool& flagMoveToNewBuffer, bool& flagBufferAllHandled) {
        if (tempHeadOfQueue == tailOfQueue.load(std::memory_order_acquire)) {
            return false;
        }
        BufferList<T>* next = tempHeadOfQueue->next.load(std::memory_order_acquire);
        BufferList<T>* prev = tempHeadOfQueue->prev;
        if (next == nullptr) {
            return false;
        }

        next->prev = prev;
        prev->next.store(next, std::memory_order_release);
        delete[] tempHeadOfQueue->currBuffer;
        tempHeadOfQueue->currBuffer = nullptr;
        tempHeadOfQueue->next.store(garbageListHead, std::memory_order_relaxed);
        garbageListHead = tempHeadOfQueue;

        tempHeadOfQueue = next;
        tempHead = tempHeadOfQueue->head;
        flagBufferAllHandled = true;
        flagMoveToNewBuffer = true;
        return true;
    }

    bool Scan(BufferList<T>*& tempHeadOfQueue, PositionIndex& tempHead, Node<T>*& tempN) {
        bool flagMoveToNewBuffer = false;
        bool flagBufferAllHandled = true;

        while (tempN->isSet.load() != State::SET) {
            tempHead++;
            if (tempN->isSet.load() != State::HANDLED) {
                flagBufferAllHandled = false;
            }

            if (tempHead >= bufferSize) {
                if (flagBufferAllHandled && flagMoveToNewBuffer) {
                    if (!fold(tempHeadOfQueue, tempHead, flagMoveToNewBuffer, flagBufferAllHandled)) {
                        return false;
                    }
                } else {
                    BufferList<T>* next = tempHeadOfQueue->next.load(std::memory_order_acquire);
                    if (next == nullptr) {
                        return false;
                    }
                    tempHeadOfQueue = next;
                    tempHead = tempHeadOfQueue->head;
                    flagBufferAllHandled = true;
                    flagMoveToNewBuffer = true;
                }
            }

            tempN = &(tempHeadOfQueue->currBuffer[tempHead]);
        }
        return true;
    }

    void ReScan(BufferList<T>* originalHeadOfQueue, BufferList<T>*& tempHeadOfQueue, PositionIndex& tempHead, Node<T>*& tempN) {
        BufferList<T>* scanHeadOfQueue = originalHeadOfQueue;
        PositionIndex scanHead = scanHeadOfQueue->head;

        while (scanHeadOfQueue != tempHeadOfQueue || scanHead < tempHead) {
            if (scanHead >= bufferSize) {
                scanHeadOfQueue = scanHeadOfQueue->next.load(std::memory_order_acquire);
                scanHead = scanHeadOfQueue->head;
                continue;
            }

            Node<T>* scanN = &(scanHeadOfQueue->currBuffer[scanHead]);
            if (scanN->isSet.load() == State::SET) {
                tempHead = scanHead;
                tempHeadOfQueue = scanHeadOfQueue;
                tempN = scanN;
                scanHeadOfQueue = originalHeadOfQueue;
                scanHead = scanHeadOfQueue->head;
                continue;
            }

            scanHead++;
        }
    }

    ~MPSCQueue() {
        BufferList<T>* current = headOfQueue;
        while (current != nullptr) {
            BufferList<T>* next = current->next.load();
            delete current;
            current = next;
        }
        current = garbageListHead;
        while (current != nullptr) {
            BufferList<T>* next = current->next.load();
            delete current;
            current = next;
        }
    }
};
