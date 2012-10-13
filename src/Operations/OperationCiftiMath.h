#ifndef __OPERATION_CIFTI_MATH_H__
#define __OPERATION_CIFTI_MATH_H__

/*LICENSE_START*/
/*
 *  Copyright 1995-2002 Washington University School of Medicine
 *
 *  http://brainmap.wustl.edu
 *
 *  This file is part of CARET.
 *
 *  CARET is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  CARET is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CARET; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
file->save as... and enter what you will name the class, plus .h

find and replace these strings, without matching "whole word only" (plain text mode):

OperationCiftiMath     : operation name, in CamelCase, with initial capital, same as what you saved the header file to
OPERATION_CIFTI_MATH    : uppercase of operation name, with underscore between words, used in #ifdef guards
-cifti-math   : switch for the command line to use, often hyphenated version of operation name, lowercase, minus "operation"
EVALUATE EXPRESSION ON CIFTI FILES : short description of the command, uppercase, three to five words, often just command switch with more verbosity

if the operation takes no parameters, uncomment the line below for takesParameters(), otherwise delete it

next, make OperationCiftiMath.cxx from AlgorithmTemplate.cxx.txt via one of the following (depending on working directory):

cat OperationTemplate.cxx.txt | sed 's/[O]perationName/OperationCiftiMath/g' | sed 's/-[c]ommand-switch/-cifti-math/g' | sed 's/[S]HORT DESCRIPTION/EVALUATE EXPRESSION ON CIFTI FILES/g' > OperationCiftiMath.cxx
cat Operations/OperationTemplate.cxx.txt | sed 's/[O]perationName/OperationCiftiMath/g' | sed 's/-[c]ommand-switch/-cifti-math/g' | sed 's/[S]HORT DESCRIPTION/EVALUATE EXPRESSION ON CIFTI FILES/g' > Operations/OperationCiftiMath.cxx
cat src/Operations/OperationTemplate.cxx.txt | sed 's/[O]perationName/OperationCiftiMath/g' | sed 's/-[c]ommand-switch/-cifti-math/g' | sed 's/[S]HORT DESCRIPTION/EVALUATE EXPRESSION ON CIFTI FILES/g' > src/Operations/OperationCiftiMath.cxx

or manually copy and replace

next, implement its functions

add these to Operations/CMakeLists.txt:

OperationCiftiMath.h
OperationCiftiMath.cxx

place the following lines into Commands/CommandOperationManager.cxx:

#include "OperationCiftiMath.h"
    //near the top

    this->commandOperations.push_back(new CommandParser(new AutoOperationCiftiMath()));
        //in CommandOperationManager()

finally, remove this block comment
*/

#include "AbstractOperation.h"

namespace caret {
    
    class OperationCiftiMath : public AbstractOperation
    {
    public:
        static OperationParameters* getParameters();
        static void useParameters(OperationParameters* myParams, ProgressObject* myProgObj);
        static AString getCommandSwitch();
        static AString getShortDescription();
    };

    typedef TemplateAutoOperation<OperationCiftiMath> AutoOperationCiftiMath;

}

#endif //__OPERATION_CIFTI_MATH_H__
