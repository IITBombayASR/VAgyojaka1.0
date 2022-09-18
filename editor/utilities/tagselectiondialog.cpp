#include "tagselectiondialog.h"
#include "ui_tagselectiondialog.h"

#include <QDebug>

TagSelectionDialog::TagSelectionDialog(QWidget* parent)
    : QDialog(parent),
    ui (new Ui::TagSelectionDialog)
{
    ui->setupUi(this);
    ui->label->setHidden(true);
    ui->comboBox_lang->setHidden(true);

    m_languages = QString("Afrikaans,Albanian,Amharic,Arabic,Armenian,Azerbaijani,Basque,Belarusian,Bengali,Bosnian,Bulgarian,Catalan,Cebuano,Corsican,Croatian,Czech,Danish,Dutch,English,Esperanto,Estonian,Finnish,French,Frisian,Galician,Georgian,German,Greek,Gujarati,Haitian Creole,Hausa,Hawaiian,Hebrew,Hindi,Hmong,Hungarian,Icelandic,Igbo,Indonesian,Irish,Italian,Japanese,Javanese,Kannada,Kazakh,Khmer,Kinyarwanda,Korean,Kurdish,Kyrgyz,Lao,Latvian,Lithuanian,Luxembourgish,Macedonian,Malagasy,Malay,Malayalam,Maltese,Maori,Marathi,Mongolian,Myanmar,Nepali,Norwegian,Nyanja,Odia (Oriya),Pashto,Persian,Polish,Portuguese,Punjabi,Romanian,Russian,Samoan,Scots Gaelic,Serbian,Sesotho,Shona,Sindhi,Sinhala,Slovak,Slovenian,Somali,Spanish,Sundanese,Swahili,Swedish,Tagalog,Tajik,Tamil,Tatar,Telugu,Thai,Turkish,Turkmen,Ukrainian,Urdu,Uyghur,Uzbek,Vietnamese,Welsh,Xhosa,Yiddish,Yoruba,Zulu").split(",");
    m_languageCodes = QString("af,sq,am,ar,hy,az,eu,be,bn,bs,bg,ca,ceb,co,hr,cs,da,nl,en,eo,et,fi,fr,fy,gl,ka,de,el,gu,ht,ha,haw,he,hi,hmn,hu,is,ig,id,ga,it,ja,jv,kn,kk,km,rw,ko,ku,ky,lo,lv,lt,lb,mk,mg,ms,ml,mt,mi,mr,mn,my,ne,no,ny,or,ps,fa,pl,pt,pa,ro,ru,sm,gd,sr,st,sn,sd,si,sk,sl,so,es,su,sw,sv,tl,tg,ta,tt,te,th,tr,tk,uk,ur,ug,uz,vi,cy,xh,yi,yo,zu").split(",");
    ui->comboBox_lang->addItems(m_languages);

    connect(ui->checkBox_lang, &QCheckBox::stateChanged, this,
    [&]()
    {
        ui->label->setVisible(!ui->label->isVisible());
        ui->comboBox_lang->setVisible(!ui->comboBox_lang->isVisible());
    }
    );
}

TagSelectionDialog::~TagSelectionDialog()
{
    delete ui;
}


QStringList TagSelectionDialog::tagList() const
{
    QStringList currentTagList;

    if (ui->checkBox_invs->isChecked())
        currentTagList << "InvS";
    if (ui->checkBox_noisy->isChecked())
        currentTagList << "Noisy";
    if (ui->checkBox_slacked->isChecked())
        currentTagList << "Slacked";
    if (ui->checkBox_l1infl->isChecked())
        currentTagList << "L1Infl";
    if (ui->checkBox_mltsp->isChecked())
        currentTagList << "MltSp";
    if (ui->checkBox_lang->isChecked() && ui->comboBox_lang->currentText() != "Select Language") {
        auto selectedLanguage = ui->comboBox_lang->currentText().split(" ").last();
        auto index = m_languages.indexOf(selectedLanguage);
        if (index != -1) {
            auto langCode = m_languageCodes.at(index);
            currentTagList << QString("Lang_%1").arg(langCode);
        }
    }

    return currentTagList;
}

void TagSelectionDialog::markExistingTags(const QStringList& existingTagsList)
{
    if (existingTagsList.contains("InvS"))
        ui->checkBox_invs->setChecked(true);
    if (existingTagsList.contains("Noisy"))
        ui->checkBox_noisy->setChecked(true);
    if (existingTagsList.contains("Slacked"))
        ui->checkBox_slacked->setChecked(true);
    if (existingTagsList.contains("L1Infl"))
        ui->checkBox_l1infl->setChecked(true);
    if (existingTagsList.contains("MltSp"))
        ui->checkBox_mltsp->setChecked(true);

    auto subList = existingTagsList.filter("Lang_");
    if (!subList.isEmpty()) {
        ui->checkBox_lang->setChecked(true);

        auto langCode = subList.first().split("_").last();
        auto language = m_languages.at(m_languageCodes.indexOf(langCode));
        ui->comboBox_lang->setCurrentText(language);
    }
}
