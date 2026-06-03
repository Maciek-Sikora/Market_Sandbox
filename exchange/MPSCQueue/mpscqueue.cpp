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
public:
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

public:
    MPSCQueue() : headOfQueue(new BufferList<T>(nullptr, 0, 1)), tailOfQueue(headOfQueue), tail(0) {}

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

    ~MPSCQueue() {
        BufferList<T>* current = headOfQueue;
        while (current != nullptr) {
            BufferList<T>* next = current->next.load();
            delete current;
            current = next;
        }
    }
};
