------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
12276           | unsigned int __cdecl CAS01ThreadFunc(void *) CAS01                                                   | 77794495        | 777             | 297             | 30252846        | 99996           |
12276           | void __cdecl CLFQueue<unsigned __int64,0>::Enqueue(unsigned __int64) noexcept Enqueue                | 29116389        | 2               | 0               | 30251756        | 12799996        |
12276           | bool __cdecl CLFQueue<unsigned __int64,0>::Dequeue(unsigned __int64 *) noexcept Dequeue              | 22811537        | 1               | 0               | 530902          | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
20560           | unsigned int __cdecl CAS01ThreadFunc(void *) CAS01                                                   | 77058526        | 770             | 300             | 30252365        | 99996           |
20560           | void __cdecl CLFQueue<unsigned __int64,0>::Enqueue(unsigned __int64) noexcept Enqueue                | 30032511        | 2               | 0               | 30251562        | 12799996        |
20560           | bool __cdecl CLFQueue<unsigned __int64,0>::Dequeue(unsigned __int64 *) noexcept Dequeue              | 23176488        | 1               | 0               | 403611          | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
9856            | unsigned int __cdecl CAS02ThreadFunc(void *) CAS02                                                   | 66868943        | 668             | 297             | 171475          | 99996           |
9856            | void __cdecl CLFQueue<unsigned __int64,1>::Enqueue(unsigned __int64) noexcept Enqueue                | 22434188        | 1               | 0               | 132730          | 12799996        |
9856            | bool __cdecl CLFQueue<unsigned __int64,1>::Dequeue(unsigned __int64 *) noexcept Dequeue              | 24986515        | 1               | 0               | 78811           | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
11928           | unsigned int __cdecl CAS02ThreadFunc(void *) CAS02                                                   | 66836564        | 668             | 297             | 117475          | 99996           |
11928           | void __cdecl CLFQueue<unsigned __int64,1>::Enqueue(unsigned __int64) noexcept Enqueue                | 22395047        | 1               | 0               | 102087          | 12799996        |
11928           | bool __cdecl CLFQueue<unsigned __int64,1>::Dequeue(unsigned __int64 *) noexcept Dequeue              | 24876401        | 1               | 0               | 65271           | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
18400           | unsigned int __cdecl stdQueueThreadFunc(void *) stdQueue                                             | 85973873        | 859             | 327             | 91336           | 99996           |
18400           | unsigned int __cdecl stdQueueThreadFunc(void *) Enqueue                                              | 35955114        | 2               | 0               | 88195           | 12799996        |
18400           | unsigned int __cdecl stdQueueThreadFunc(void *) Dequeue                                              | 36324233        | 2               | 0               | 59661           | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
12308           | unsigned int __cdecl stdQueueThreadFunc(void *) stdQueue                                             | 86465882        | 864             | 327             | 102429          | 99996           |
12308           | unsigned int __cdecl stdQueueThreadFunc(void *) Enqueue                                              | 36279893        | 2               | 0               | 43235           | 12799996        |
12308           | unsigned int __cdecl stdQueueThreadFunc(void *) Dequeue                                              | 36435637        | 2               | 0               | 100735          | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------