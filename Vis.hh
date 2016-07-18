////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 07 Sep 2015 Paul J. Gartside                                 //
////////////////////////////////////////////////////////////////////////////////
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without  limitation //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
////////////////////////////////////////////////////////////////////////////////

#ifndef __VIS_HH__
#define __VIS_HH__

class String;

class Vis
{
public:
  Vis();
  ~Vis();

  void Init( const int ARGC, const char* const ARGV[] );
  void Run();
  void Stop();
  View* CV() const;
  View* WinView( const unsigned w ) const;
  unsigned    GetNumWins() const;
  Paste_Mode  GetPasteMode() const;
  void        SetPasteMode( Paste_Mode pm );
  bool        InDiffMode() const;
  bool        RunningDot() const;
  bool        RunningCmd() const;
  FileBuf*    GetFileBuf( const unsigned index ) const;
  unsigned    GetStarLen() const;
  const char* GetStar() const;
  bool        GetSlash() const;

  void CheckWindowSize();
  void CheckFileModTime();
  void Add_FileBuf_2_Lists_Create_Views( FileBuf* pfb, const char* fname );
  void CmdLineMessage( const char* const msg_fmt, ... );
  void Window_Message( const char* const msg_fmt, ... );
  void UpdateAll();
  bool Update_Status_Lines();
  bool Update_Change_Statuses();
  void PrintCursor();
  bool HaveFile( const char* file_name, unsigned* file_index=0 );
  bool File_Is_Displayed( const String& full_fname );
  void ReleaseFileName( const String& full_fname );
  bool GoToBuffer_Fname( String& fname );
  void Handle_f();
  void Handle_z();
  void Handle_SemiColon();
  void Handle_Slash_GotPattern( const String& pattern
                              , const bool MOVE_TO_FIRST_PATTERN=true );

  Line* BorrowLine( const char* _FILE_, const unsigned _LINE_, const unsigned SIZE = 0 );
  Line* BorrowLine( const char* _FILE_, const unsigned _LINE_, const unsigned LEN, const uint8_t FILL );
  Line* BorrowLine( const char* _FILE_, const unsigned _LINE_, const Line& line );
  void  ReturnLine( Line* lp );

  LineChange* BorrowLineChange( const ChangeType type
                              , const unsigned   lnum
                              , const unsigned   cpos );
  void  ReturnLineChange( LineChange* lcp );

  struct Data;

private:
  Data& m;
};

#endif

