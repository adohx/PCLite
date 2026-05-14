//
// Created by cj on 2026-05-13.
//

#ifndef PCLITE_LASREADER_H
#define PCLITE_LASREADER_H

#include "attribute_reader.h"
#include "las_reader.h"

class LasReader:public AttributeReader{
public:
    LasReader();
    ~LasReader();


public:
    int getReaderType(){return AttributeReader::PCD;}
};


#endif //PCLITE_LASREADER_H
