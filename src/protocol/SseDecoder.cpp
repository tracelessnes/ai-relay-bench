#include "SseDecoder.h"

namespace airb {
QList<SseEvent> SseDecoder::feed(const QByteArray& b){
    totalBytes_+=b.size();buffer_+=b;
    if(buffer_.size()>MaxBuffer){warning_="SSE buffer exceeded 8 MiB; truncated";buffer_=buffer_.right(MaxBuffer);}
    return consume(false);
}
QList<SseEvent> SseDecoder::finish(){return consume(true);}
void SseDecoder::reset(){buffer_.clear();event_.clear();data_.clear();id_.clear();totalBytes_=0;warning_.clear();}
QList<SseEvent> SseDecoder::consume(bool final){
    QList<SseEvent> out;
    while(true){
        qsizetype p=-1,sep=1;
        for(qsizetype i=0;i<buffer_.size();++i){
            if(buffer_[i]!='\n'&&buffer_[i]!='\r')continue;
            if(!final&&buffer_[i]=='\r'&&i+1==buffer_.size())return out;
            p=i;if(buffer_[i]=='\r'&&i+1<buffer_.size()&&buffer_[i+1]=='\n')sep=2;break;
        }
        if(p<0)break;
        const QByteArray line=buffer_.left(p);buffer_.remove(0,p+sep);processLine(line,out);
    }
    if(final){if(!buffer_.isEmpty()){processLine(buffer_,out);buffer_.clear();}if(!data_.isEmpty()||!event_.isEmpty())dispatch(out);}
    return out;
}
void SseDecoder::processLine(QByteArray line,QList<SseEvent>& out){if(line.isEmpty()){dispatch(out);return;}if(line.startsWith(':'))return;const auto p=line.indexOf(':');const QByteArray field=p<0?line:line.left(p);QByteArray value=p<0?QByteArray():line.mid(p+1);if(value.startsWith(' '))value.remove(0,1);if(field=="event")event_=value;else if(field=="data"){if(!data_.isEmpty())data_+='\n';data_+=value;}else if(field=="id")id_=value;}
void SseDecoder::dispatch(QList<SseEvent>& out){if(data_.isEmpty()&&event_.isEmpty()){id_.clear();return;}out.push_back({event_,data_,id_});event_.clear();data_.clear();id_.clear();}
}
