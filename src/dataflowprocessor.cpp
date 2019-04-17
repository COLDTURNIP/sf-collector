#include "dataflowprocessor.h"

using namespace dataflow;

DataFlowProcessor::DataFlowProcessor(SysFlowContext* cxt, SysFlowWriter* writer, process::ProcessContext* processCxt, file::FileContext* fileCxt) : m_dfSet() {
    m_cxt = cxt;
    m_netflowPrcr = new networkflow::NetworkFlowProcessor(cxt, writer, processCxt, &m_dfSet);
    m_fileflowPrcr = new fileflow::FileFlowProcessor(cxt, writer, processCxt, &m_dfSet, fileCxt);
    m_fileevtPrcr = new fileevent::FileEventProcessor(writer, processCxt, fileCxt);
    m_lastCheck = 0;
}

DataFlowProcessor::~DataFlowProcessor() {
   //clearTables();
    if(m_netflowPrcr != NULL) {
        delete m_netflowPrcr;
    }
    if(m_fileflowPrcr != NULL) {
        delete m_fileflowPrcr;
    }
    if(m_fileevtPrcr != NULL) {
        delete m_fileevtPrcr;
    }
}


int DataFlowProcessor::handleDataEvent(sinsp_evt* ev, OpFlags flag) {
    sinsp_fdinfo_t * fdinfo = ev->get_fd_info();
   // if(fdinfo == NULL && ev->get_type() == PPME_SYSCALL_SELECT_X) {
   //   return 0;
   //}

    if(fdinfo == NULL) {
       cout << "Uh oh!!! Event: " << ev->get_name() << " doesn't have an fdinfo associated with it! ErrorCode: " << utils::getSyscallResult(ev) << endl;
       if(IS_ATOMIC_FILE_FLOW(flag)) {
           return m_fileevtPrcr->handleFileFlowEvent(ev, flag);
       }
       return 1;
    }
    if(fdinfo->is_ipv4_socket() || fdinfo->is_ipv6_socket()) {
      return m_netflowPrcr->handleNetFlowEvent(ev, flag);
    } else if(IS_ATOMIC_FILE_FLOW(flag)) {
      return m_fileevtPrcr->handleFileFlowEvent(ev, flag);
    } else {
      return m_fileflowPrcr->handleFileFlowEvent(ev, flag);
    }
    return 2;
}

int DataFlowProcessor::removeAndWriteDFFromProc(ProcessObj* proc, int64_t tid) {
   int total = m_fileflowPrcr->removeAndWriteFFFromProc(proc, tid);
   return (total + m_netflowPrcr->removeAndWriteNFFromProc(proc, tid));
}


int DataFlowProcessor::checkForExpiredRecords() {
     time_t now = utils::getCurrentTime(m_cxt);
     if(m_lastCheck == 0) {
        m_lastCheck = now;
        return 0;
     }
     if(difftime(now, m_lastCheck) < 1.0) {
        return 0;
     }
     m_lastCheck = now;
     int i = 0;
     cout << "Checking expired Flows!!!...." << endl;
     for(DataFlowSet::iterator it = m_dfSet.begin(); it != m_dfSet.end(); ) {
             cout << "Checking flow with exportTime: " << (*it)->exportTime << " Now: " << now << endl;
            if((*it)->exportTime <= now) {
                 cout << "Exporting flow!!! " << endl; 
                if(difftime(now, (*it)->lastUpdate) >= m_cxt->getNFExpireInterval()) {
                    if((*it)->isNetworkFlow) {
                         m_netflowPrcr->removeNetworkFlow((*it));
                    }else {
                         m_fileflowPrcr->removeFileFlow((*it));
                    }
                    it = m_dfSet.erase(it);
                } else {
                    if((*it)->isNetworkFlow) {
                         m_netflowPrcr->exportNetworkFlow((*it), now);
                    }else {
                         m_fileflowPrcr->exportFileFlow((*it), now);
                    }
                    DataFlowObj* dfo = (*it);
                    it = m_dfSet.erase(it);
                    dfo->exportTime = utils::getExportTime(m_cxt);
                    m_dfSet.insert(dfo);
                }
                i++;
            }else {
               break;
            }
     }
     return i;
}
