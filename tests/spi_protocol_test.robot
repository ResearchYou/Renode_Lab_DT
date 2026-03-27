*** Settings ***
Documentation     Dual STM32F746 custom SPI protocol test.
...               Validates packet exchange with checksums, ordering, and DMA.
Library           OperatingSystem
Library           Process

Suite Setup       Run Renode
Suite Teardown    Clean Up

*** Variables ***
${RENODE_SCRIPT}    /workspace/renode/scripts/spi_test.resc
${OUTPUT_DIR}       /workspace/output
${MASTER_UART}      ${OUTPUT_DIR}/master_uart.txt
${SLAVE_UART}       ${OUTPUT_DIR}/slave_uart.txt
${RENODE_LOG}       ${OUTPUT_DIR}/renode.log

*** Keywords ***
Run Renode
    Create Directory    ${OUTPUT_DIR}
    ${result}=    Run Process
    ...    renode    --disable-xwt    --console    ${RENODE_SCRIPT}
    ...    timeout=60s    stdout=${OUTPUT_DIR}/renode_stdout.txt
    ...    stderr=${OUTPUT_DIR}/renode_stderr.txt
    Log    Renode exit code: ${result.rc}

Clean Up
    No Operation

*** Test Cases ***
Master Completes All Packet Exchanges
    [Documentation]    Master sends 10 packets and receives valid responses.
    File Should Exist    ${MASTER_UART}
    ${content}=    Get File    ${MASTER_UART}
    Should Contain    ${content}    MASTER: ALL TESTS PASSED

Slave Validates All Received Packets
    [Documentation]    Slave validates checksum and protocol for all 10 packets.
    File Should Exist    ${SLAVE_UART}
    ${content}=    Get File    ${SLAVE_UART}
    Should Contain    ${content}    SLAVE: ALL TESTS PASSED

Master Validates Packet Ordering
    [Documentation]    Sequence numbers 1-10 are acknowledged in order.
    ${content}=    Get File    ${MASTER_UART}
    FOR    ${n}    IN RANGE    1    11
        Should Contain    ${content}    MASTER: PKT ${n} ACK OK
    END

Slave Validates Packet Ordering
    [Documentation]    Slave processes packets 1-10 with correct checksums.
    ${content}=    Get File    ${SLAVE_UART}
    FOR    ${n}    IN RANGE    1    11
        Should Contain    ${content}    SLAVE: PKT ${n} OK
    END

No Checksum Failures On Master
    [Documentation]    No ACK FAIL lines in master output.
    ${content}=    Get File    ${MASTER_UART}
    Should Not Contain    ${content}    ACK FAIL

No Checksum Failures On Slave
    [Documentation]    No FAIL lines in slave output.
    ${content}=    Get File    ${SLAVE_UART}
    Should Not Contain    ${content}    FAIL

SPI Line Usage Is Correct
    [Documentation]    Bridge logs confirm packet routing and transfer counts.
    File Should Exist    ${RENODE_LOG}
    ${content}=    Get File    ${RENODE_LOG}
    Should Contain    ${content}    SPI_LINE: TX_PACKET seq=1 delivered to slave
    Should Contain    ${content}    SPI_LINE: TX_PACKET seq=10 delivered to slave
    ${tx_count}=    Get Count    ${content}    SPI_LINE: TX_PACKET
    ${rx_count}=    Get Count    ${content}    SPI_LINE: RX_TRANSFER complete
    Should Be Equal As Integers    ${tx_count}    10
    Should Be Equal As Integers    ${rx_count}    10

SPI Bus Values Are Correct
    [Documentation]    Bridge logs confirm captured MOSI and MISO packet bytes.
    File Should Exist    ${RENODE_LOG}
    ${content}=    Get File    ${RENODE_LOG}
    Should Contain    ${content}    SPI_BUS: MOSI_REQ seq=1 frame=AA 01 01 10 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 AA
    Should Contain    ${content}    SPI_BUS: MOSI_REQ seq=10 frame=AA 0A 01 10 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 B1
    Should Contain    ${content}    SPI_BUS: MISO_RSP seq=1 frame=AA 01 81 10 AC 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 96
    Should Contain    ${content}    SPI_BUS: MISO_RSP seq=10 frame=AA 0A 81 10 AC 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 84
    ${mosi_count}=    Get Count    ${content}    SPI_BUS: MOSI_REQ
    ${miso_count}=    Get Count    ${content}    SPI_BUS: MISO_RSP
    Should Be Equal As Integers    ${mosi_count}    10
    Should Be Equal As Integers    ${miso_count}    10
