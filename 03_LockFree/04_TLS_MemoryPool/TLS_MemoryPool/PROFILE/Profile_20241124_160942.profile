------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
2148            | unsigned int __cdecl CAS01ThreadFunc(void *) CAS01                                                   | 65547880        | 655             | 280             | 616941          | 99996           |
2148            | void __cdecl CLFQueue<unsigned __int64,0>::Enqueue(unsigned __int64) noexcept Enqueue                | 26424428        | 2               | 0               | 616215          | 12799996        |
2148            | bool __cdecl CLFQueue<unsigned __int64,0>::Dequeue(unsigned __int64 *) noexcept Dequeue              | 15993135        | 1               | 0               | 286473          | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
13560           | unsigned int __cdecl CAS01ThreadFunc(void *) CAS01                                                   | 65445728        | 654             | 282             | 426581          | 99996           |
13560           | void __cdecl CLFQueue<unsigned __int64,0>::Enqueue(unsigned __int64) noexcept Enqueue                | 26004192        | 2               | 0               | 426119          | 12799996        |
13560           | bool __cdecl CLFQueue<unsigned __int64,0>::Dequeue(unsigned __int64 *) noexcept Dequeue              | 15841216        | 1               | 0               | 297922          | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
4284            | unsigned int __cdecl CAS02ThreadFunc(void *) CAS02                                                   | 61017551        | 610             | 292             | 57233           | 99996           |
4284            | void __cdecl CLFQueue<unsigned __int64,1>::Enqueue(unsigned __int64) noexcept Enqueue                | 21103586        | 1               | 0               | 24184           | 12799996        |
4284            | bool __cdecl CLFQueue<unsigned __int64,1>::Dequeue(unsigned __int64 *) noexcept Dequeue              | 22452251        | 1               | 0               | 37558           | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
11152           | unsigned int __cdecl CAS02ThreadFunc(void *) CAS02                                                   | 61225724        | 612             | 291             | 62053           | 99996           |
11152           | void __cdecl CLFQueue<unsigned __int64,1>::Enqueue(unsigned __int64) noexcept Enqueue                | 21647497        | 1               | 0               | 28858           | 12799996        |
11152           | bool __cdecl CLFQueue<unsigned __int64,1>::Dequeue(unsigned __int64 *) noexcept Dequeue              | 22293348        | 1               | 0               | 59315           | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
14596           | unsigned int __cdecl stdQueueThreadFunc(void *) stdQueue                                             | 80156911        | 801             | 325             | 63058           | 99996           |
14596           | unsigned int __cdecl stdQueueThreadFunc(void *) Enqueue                                              | 30904470        | 2               | 0               | 60793           | 12799996        |
14596           | unsigned int __cdecl stdQueueThreadFunc(void *) Dequeue                                              | 36846432        | 2               | 0               | 28794           | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ThreadId        | Name                                                                                                 | TotalTime(ns)   | Average(ns)     | Min(ns)         | Max(ns)         | Call            |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
17172           | unsigned int __cdecl stdQueueThreadFunc(void *) stdQueue                                             | 81584376        | 815             | 325             | 61880           | 99996           |
17172           | unsigned int __cdecl stdQueueThreadFunc(void *) Enqueue                                              | 31331288        | 2               | 0               | 60639           | 12799996        |
17172           | unsigned int __cdecl stdQueueThreadFunc(void *) Dequeue                                              | 37677484        | 2               | 0               | 47444           | 12799996        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------