#ifndef DEFOCAMERAMODEL_H
#define DEFOCAMERAMODEL_H

#include <QObject>
#include <QImage>
#include <QFile>

#include <map>

// TODO USE_FAKEIO
#include "../devices/Canon/EOS550D.h"
typedef EOS550D EOS550D_t;

#include <QImage>
#include "DefoState.h"

class DefoCameraModel :
      public QObject
    , public DefoAbstractDeviceModel<EOS550D_t>
{
    Q_OBJECT
public:
  typedef std::vector<std::string> OptionList;
  /// Copy enum values from VEOS500D to not expose them directly.
  enum Option {
      APERTURE      = EOS550D::APERTURE
    , SHUTTER_SPEED = EOS550D::SHUTTER_SPEED
    , ISO           = EOS550D::ISO
    , WHITE_BALANCE = EOS550D::WHITE_BALANCE
  };

  explicit DefoCameraModel(QObject *parent = 0);

  std::vector<std::string> getOptions( const Option& option ) const;
  void setOptionSelection( const Option& option, int value );
  int getOptionValue( const Option& option) const;

  const QImage& getLastPicture() const;
  const QString& getLastPictureLocation() const;
  // TODO getLastPictureExif()

public slots:
  virtual void setDeviceEnabled( bool enabled );
  void acquirePicture();

protected:
  virtual void initialize();
  virtual void setDeviceState( State state );

  // parameter cache
  std::map<Option, int> parameters_;
  void resetParameterCache();

  // image cache
  QString location_;
  QImage image_;

signals:
  void deviceStateChanged(State newState);
  void deviceOptionChanged(DefoCameraModel::Option option, int newValue);
  void newImage(QString location);

};

#endif // DEFOCAMERAMODEL_H