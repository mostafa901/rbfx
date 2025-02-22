//
// Copyright (c) 2008-2019 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include <EASTL/list.h>
#include <atomic>

#include "../Core/Mutex.h"
#include "../Core/Object.h"

namespace Urho3D
{

/// Work item completed event.
URHO3D_EVENT(E_WORKITEMCOMPLETED, WorkItemCompleted)
{
    URHO3D_PARAM(P_ITEM, Item);                        // WorkItem ptr
}

class WorkerThread;

/// Work queue item.
struct WorkItem : public RefCounted
{
    friend class WorkQueue;

public:
    /// Work function. Called with the work item and thread index (0 = main thread) as parameters.
    void (* workFunction_)(const WorkItem*, unsigned){};
    /// Data start pointer.
    void* start_{};
    /// Data end pointer.
    void* end_{};
    /// Auxiliary data pointer.
    void* aux_{};
    /// Priority. Higher value = will be completed first.
    unsigned priority_{};
    /// Whether to send event on completion.
    bool sendEvent_{};
    /// Completed flag.
    std::atomic<bool> completed_{};

private:
    bool pooled_{};
    /// Work function. Called without any parameters.
    std::function<void()> workLambda_;
};

/// Work queue subsystem for multithreading.
class URHO3D_API WorkQueue : public Object
{
    URHO3D_OBJECT(WorkQueue, Object);

    friend class WorkerThread;

public:
    /// Construct.
    explicit WorkQueue(Context* context);
    /// Destruct.
    ~WorkQueue() override;

    /// Create worker threads. Can only be called once.
    void CreateThreads(unsigned numThreads);
    /// Get pointer to an usable WorkItem from the item pool. Allocate one if no more free items.
    SharedPtr<WorkItem> GetFreeItem();
    /// Add a work item and resume worker threads.
    void AddWorkItem(const SharedPtr<WorkItem>& item);
    /// Add a work item and resume worker threads.
    SharedPtr<WorkItem> AddWorkItem(std::function<void()> workFunction, unsigned priority = 0);
    /// Remove a work item before it has started executing. Return true if successfully removed.
    bool RemoveWorkItem(SharedPtr<WorkItem> item);
    /// Remove a number of work items before they have started executing. Return the number of items successfully removed.
    unsigned RemoveWorkItems(const ea::vector<SharedPtr<WorkItem> >& items);
    /// Pause worker threads.
    void Pause();
    /// Resume worker threads.
    void Resume();
    /// Finish all queued work which has at least the specified priority. Main thread will also execute priority work. Pause worker threads if no more work remains.
    void Complete(unsigned priority);

    /// Set the pool telerance before it starts deleting pool items.
    void SetTolerance(int tolerance) { tolerance_ = tolerance; }

    /// Set how many milliseconds maximum per frame to spend on low-priority work, when there are no worker threads.
    void SetNonThreadedWorkMs(int ms) { maxNonThreadedWorkMs_ = Max(ms, 1); }

    /// Return number of worker threads.
    unsigned GetNumThreads() const { return threads_.size(); }

    /// Return number of incomplete tasks with at least the specified priority.
    unsigned GetNumIncomplete(unsigned priority) const;
    /// Return whether all work with at least the specified priority is finished.
    bool IsCompleted(unsigned priority) const;
    /// Return whether the queue is currently completing work in the main thread.
    bool IsCompleting() const { return completing_; }

    /// Return the pool tolerance.
    int GetTolerance() const { return tolerance_; }

    /// Return how many milliseconds maximum to spend on non-threaded low-priority work.
    int GetNonThreadedWorkMs() const { return maxNonThreadedWorkMs_; }

private:
    /// Process work items until shut down. Called by the worker threads.
    void ProcessItems(unsigned threadIndex);
    /// Purge completed work items which have at least the specified priority, and send completion events as necessary.
    void PurgeCompleted(unsigned priority);
    /// Purge the pool to reduce allocation where its unneeded.
    void PurgePool();
    /// Return a work item to the pool.
    void ReturnToPool(SharedPtr<WorkItem>& item);
    /// Handle frame start event. Purge completed work from the main thread queue, and perform work if no threads at all.
    void HandleBeginFrame(StringHash eventType, VariantMap& eventData);

    /// Worker threads.
    ea::vector<SharedPtr<WorkerThread> > threads_;
    /// Work item pool for reuse to cut down on allocation. The bool is a flag for item pooling and whether it is available or not.
    ea::list<SharedPtr<WorkItem> > poolItems_;
    /// Work item collection. Accessed only by the main thread.
    ea::list<SharedPtr<WorkItem> > workItems_;
    /// Work item prioritized queue for worker threads. Pointers are guaranteed to be valid (point to workItems.)
    ea::list<WorkItem*> queue_;
    /// Worker queue mutex.
    Mutex queueMutex_;
    /// Shutting down flag.
    std::atomic<bool> shutDown_;
    /// Pausing flag. Indicates the worker threads should not contend for the queue mutex.
    std::atomic<bool> pausing_;
    /// Paused flag. Indicates the queue mutex being locked to prevent worker threads using up CPU time.
    bool paused_;
    /// Completing work in the main thread flag.
    bool completing_;
    /// Tolerance for the shared pool before it begins to deallocate.
    int tolerance_;
    /// Last size of the shared pool.
    unsigned lastSize_;
    /// Maximum milliseconds per frame to spend on low-priority work, when there are no worker threads.
    int maxNonThreadedWorkMs_;
};

}
