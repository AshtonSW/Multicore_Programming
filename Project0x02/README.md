# Multicore Programming(System programming)
# Sogang University(서강대학교)
    Course ID:  CSE4100 
# Project 2: Concurrent Stock Server

    1) Task 1: Event-driven Approach 
       단일 thread를 통해서 모든 client의 요청을 처리하기 위해서 사용되는 
      event-driven approach에서 select 함수를 사용해서 처리했다. 
      Select() 함수는 여러 socket의 상태를 확인하고, 한 개 이상의 I/O 연산이 완료될 때까지 
      프로그램을 블록 한다. 이를 I/O multiplexing이라고 한다. 
       I/O multiplexing은 하나의 process나 thread가 여러 개의 I/O 요청을 동시에 처리한다.
      서버는 각 client의 상태를 계속 확인하고, client로부터 데이터가 도착하면 그것을 읽고 
      적절하게 응답을 제공한다. 이를 위해서는 서버는 모든 client socket을 감시하며, 데이터가 
      준비된 socket을 찾아서 읽는다. 

    2) Task 2: Thread-based Approach
       Master thread를 통해서 client의 요청을 받아들이고 관리한다. 새로운 client가 연결을 
      요청하면 master thread는 해당 연결을 받아들이면서 새로운 socket을 생성한다. 이후 이 
      socket를 처리할 때 worker thread를 선택하고 할당한다. 이렇게 하여 master thread는 
      다수의 동시 연결을 관리하고, 각 연결을 적절한 worker thread에 분배한다. 

    3) Task 3: Performance Evaluation 
       Elapsed time: Client들의 요청을 처리하는데 걸리는 전체 시간을 측정한다.
       Concurrent throughput(동시 처리율): 서버가 동시에 처리할 수 있는 요청의 수이다. 
        이 지표는 서버의 멀티태스킹 능력을 평가할 수 있고, 서버가 client의 요청을 동시에 
        처리할 수 있다면 이는 더 높은 처리량을 가짐을 의미 

<실행 방법>

Server -> ./stockserver [port #]

Client -> ./multiclient [IP_addr] [port #] [client_num]
